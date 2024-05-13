/*-------------------------------------------------------------------------
 *
 * auth-scram.c
 *	  Server-side implementation of the SASL SCRAM-SHA-256 mechanism.
 *
 * See the following RFCs for more details:
 * - RFC 5802: https://tools.ietf.org/html/rfc5802
 * - RFC 5803: https://tools.ietf.org/html/rfc5803
 * - RFC 7677: https://tools.ietf.org/html/rfc7677
 *
 * Here are some differences:
 *
 * - Username from the authentication exchange is not used. The client
 *	 should send an empty string as the username.
 *
 * - If the password isn't valid UTF-8, or contains characters prohibited
 *	 by the SASLprep profile, we skip the SASLprep pre-processing and use
 *	 the raw bytes in calculating the hash.
 *
 * - If channel binding is used, the channel binding type is always
 *	 "tls-server-end-point".  The spec says the default is "tls-unique"
 *	 (RFC 5802, section 6.1. Default Channel Binding), but there are some
 *	 problems with that.  Firstly, not all SSL libraries provide an API to
 *	 get the TLS Finished message, required to use "tls-unique".  Secondly,
 *	 "tls-unique" is not specified for TLS v1.3, and as of this writing,
 *	 it's not clear if there will be a replacement.  We could support both
 *	 "tls-server-end-point" and "tls-unique", but for our use case,
 *	 "tls-unique" doesn't really have any advantages.  The main advantage
 *	 of "tls-unique" would be that it works even if the server doesn't
 *	 have a certificate, but PostgreSQL requires a server certificate
 *	 whenever SSL is used, anyway.
 *
 *
 * The password stored in pg_authid consists of the iteration count, salt,
 * StoredKey and ServerKey.
 *
 * SASLprep usage
 * --------------
 *
 * One notable difference to the SCRAM specification is that while the
 * specification dictates that the password is in UTF-8, and prohibits
 * certain characters, we are more lenient.  If the password isn't a valid
 * UTF-8 string, or contains prohibited characters, the raw bytes are used
 * to calculate the hash instead, without SASLprep processing.  This is
 * because PostgreSQL supports other encodings too, and the encoding being
 * used during authentication is undefined (client_encoding isn't set until
 * after authentication).  In effect, we try to interpret the password as
 * UTF-8 and apply SASLprep processing, but if it looks invalid, we assume
 * that it's in some other encoding.
 *
 * In the worst case, we misinterpret a password that's in a different
 * encoding as being Unicode, because it happens to consists entirely of
 * valid UTF-8 bytes, and we apply Unicode normalization to it.  As long
 * as we do that consistently, that will not lead to failed logins.
 * Fortunately, the UTF-8 byte sequences that are ignored by SASLprep
 * don't correspond to any commonly used characters in any of the other
 * supported encodings, so it should not lead to any significant loss in
 * entropy, even if the normalization is incorrectly applied to a
 * non-UTF-8 password.
 *
 * Error handling
 * --------------
 *
 * Don't reveal user information to an unauthenticated client.  We don't
 * want an attacker to be able to probe whether a particular username is
 * valid.  In SCRAM, the server has to read the salt and iteration count
 * from the user's password verifier, and send it to the client.  To avoid
 * revealing whether a user exists, when the client tries to authenticate
 * with a username that doesn't exist, or doesn't have a valid SCRAM
 * verifier in pg_authid, we create a fake salt and iteration count
 * on-the-fly, and proceed with the authentication with that.  In the end,
 * we'll reject the attempt, as if an incorrect password was given.  When
 * we are performing a "mock" authentication, the 'doomed' flag in
 * scram_state is set.
 *
 * In the error messages, avoid printing strings from the client, unless
 * you check that they are pure ASCII.  We don't want an unauthenticated
 * attacker to be able to spam the logs with characters that are not valid
 * to the encoding being used, whatever that is.  We cannot avoid that in
 * general, after logging in, but let's do what we can here.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/libpq/auth-scram.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "access/xlog.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_control.h"
#include "common/base64.h"
#include "common/saslprep.h"
#include "common/scram-common.h"
#include "common/sha2.h"
#include "libpq/auth.h"
#include "libpq/crypt.h"
#include "libpq/scram.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

/*
 * Status data for a SCRAM authentication exchange.  This should be kept
 * internal to this file.
 */
typedef enum {
  SCRAM_AUTH_INIT,
  SCRAM_AUTH_SALT_SENT,
  SCRAM_AUTH_FINISHED
} scram_state_enum;

typedef struct {
  scram_state_enum state;

  const char *username; /* username from startup packet */

  Port *port;
  bool channel_binding_in_use;

  int iterations;
  char *salt; /* base64-encoded */
  uint8 StoredKey[SCRAM_KEY_LEN];
  uint8 ServerKey[SCRAM_KEY_LEN];

  /* Fields of the first message from client */
  char cbind_flag;
  char *client_first_message_bare;
  char *client_username;
  char *client_nonce;

  /* Fields from the last message from client */
  char *client_final_message_without_proof;
  char *client_final_nonce;
  char ClientProof[SCRAM_KEY_LEN];

  /* Fields generated in the server */
  char *server_first_message;
  char *server_nonce;

  /*
   * If something goes wrong during the authentication, or we are performing
   * a "mock" authentication (see comments at top of file), the 'doomed'
   * flag is set.  A reason for the failure, for the server log, is put in
   * 'logdetail'.
   */
  bool doomed;
  char *logdetail;
} scram_state;

static void
read_client_first_message(scram_state *state, const char *input);
static void
read_client_final_message(scram_state *state, const char *input);
static char *
build_server_first_message(scram_state *state);
static char *
build_server_final_message(scram_state *state);
static bool
verify_client_proof(scram_state *state);
static bool
verify_final_nonce(scram_state *state);
static void
mock_scram_verifier(const char *username, int *iterations, char **salt,
                    uint8 *stored_key, uint8 *server_key);
static bool
is_scram_printable(char *p);
static char *
sanitize_char(char c);
static char *
sanitize_str(const char *s);
static char *
scram_mock_salt(const char *username);

/*
 * pg_be_scram_get_mechanisms
 *
 * Get a list of SASL mechanisms that this module supports.
 *
 * For the convenience of building the FE/BE packet that lists the
 * mechanisms, the names are appended to the given StringInfo buffer,
 * separated by '\0' bytes.
 */
void
pg_be_scram_get_mechanisms(Port *port, StringInfo buf)
{














}

/*
 * pg_be_scram_init
 *
 * Initialize a new SCRAM authentication exchange status tracker.  This
 * needs to be called before doing any exchange.  It will be filled later
 * after the beginning of the exchange with verifier data.
 *
 * 'selected_mech' identifies the SASL mechanism that the client selected.
 * It should be one of the mechanisms that we support, as returned by
 * pg_be_scram_get_mechanisms().
 *
 * 'shadow_pass' is the role's password verifier, from pg_authid.rolpassword.
 * The username was provided by the client in the startup message, and is
 * available in port->user_name.  If 'shadow_pass' is NULL, we still perform
 * an authentication exchange, but it will fail, as if an incorrect password
 * was given.
 */
void *
pg_be_scram_init(Port *port, const char *selected_mech, const char *shadow_pass)
{

















































































}

/*
 * Continue a SCRAM authentication exchange.
 *
 * 'input' is the SCRAM payload sent by the client.  On the first call,
 * 'input' contains the "Initial Client Response" that the client sent as
 * part of the SASLInitialResponse message, or NULL if no Initial Client
 * Response was given.  (The SASL specification distinguishes between an
 * empty response and non-existing one.)  On subsequent calls, 'input'
 * cannot be NULL.  For convenience in this function, the caller must
 * ensure that there is a null terminator at input[inputlen].
 *
 * The next message to send to client is saved in 'output', for a length
 * of 'outputlen'.  In the case of an error, optionally store a palloc'd
 * string at *logdetail that will be sent to the postmaster log (but not
 * the client).
 */
int
pg_be_scram_exchange(void *opaq, const char *input, int inputlen, char **output,
                     int *outputlen, char **logdetail)
{















































































































}

/*
 * Construct a verifier string for SCRAM, stored in pg_authid.rolpassword.
 *
 * The result is palloc'd, so caller is responsible for freeing it.
 */
char *
pg_be_scram_build_verifier(const char *password)
{





























}

/*
 * Verify a plaintext password against a SCRAM verifier.  This is used when
 * performing plaintext password authentication for a user that has a SCRAM
 * verifier stored in pg_authid.
 */
bool
scram_verify_plain_password(const char *username, const char *password,
                            const char *verifier)
{














































}

/*
 * Parse and validate format of given SCRAM verifier.
 *
 * On success, the iteration count, salt, stored key, and server key are
 * extracted from the verifier, and returned to the caller.  For 'stored_key'
 * and 'server_key', the caller must pass pre-allocated buffers of size
 * SCRAM_KEY_LEN.  Salt is returned as a base64-encoded, null-terminated
 * string.  The buffer for the salt is palloc'd by this function.
 *
 * Returns true if the SCRAM verifier has been parsed, and false otherwise.
 */
bool
parse_scram_verifier(const char *verifier, int *iterations, char **salt,
                     uint8 *stored_key, uint8 *server_key)
{
















































































}

/*
 * Generate plausible SCRAM verifier parameters for mock authentication.
 *
 * In a normal authentication, these are extracted from the verifier
 * stored in the server.  This function generates values that look
 * realistic, for when there is no stored verifier.
 *
 * Like in parse_scram_verifier(), for 'stored_key' and 'server_key', the
 * caller must pass pre-allocated buffers of size SCRAM_KEY_LEN, and
 * the buffer for the salt is palloc'd by this function.
 */
static void
mock_scram_verifier(const char *username, int *iterations, char **salt,
                    uint8 *stored_key, uint8 *server_key)
{

















}

/*
 * Read the value in a given SCRAM exchange message for given attribute.
 */
static char *
read_attr_value(char **input, char attr)
{
































}

static bool
is_scram_printable(char *p)
{















}

/*
 * Convert an arbitrary byte to printable form.  For error messages.
 *
 * If it's a printable ASCII character, print it as a single character.
 * otherwise, print it in hex.
 *
 * The returned pointer points to a static buffer.
 */
static char *
sanitize_char(char c)
{








}

/*
 * Convert an arbitrary string to printable form, for error messages.
 *
 * Anything that's not a printable ASCII character is replaced with
 * '?', and the string is truncated at 30 characters.
 *
 * The returned pointer points to a static buffer.
 */
static char *
sanitize_str(const char *s)
{


















}

/*
 * Read the next attribute and value in a SCRAM exchange message.
 *
 * Returns NULL if there is attribute.
 */
static char *
read_any_attr(char **input, char *attr_p)
{











































}

/*
 * Read and parse the first message from client in the context of a SCRAM
 * authentication exchange message.
 *
 * At this stage, any errors will be reported directly with ereport(ERROR).
 */
static void
read_client_first_message(scram_state *state, const char *input)
{





































































































































































































































}

/*
 * Verify the final nonce contained in the last message received from
 * client in an exchange.
 */
static bool
verify_final_nonce(scram_state *state)
{

















}

/*
 * Verify the client proof contained in the last message received from
 * client in an exchange.
 */
static bool
verify_client_proof(scram_state *state)
{































}

/*
 * Build the first server-side message sent to the client in a SCRAM
 * communication exchange.
 */
static char *
build_server_first_message(scram_state *state)
{
















































}

/*
 * Read and parse the final message received from client.
 */
static void
read_client_final_message(scram_state *state, const char *input)
{




































































































































}

/*
 * Build the final server-side message of an exchange.
 */
static char *
build_server_final_message(scram_state *state)
{


































}

/*
 * Deterministically generate salt for mock authentication, using a SHA256
 * hash based on the username and a cluster-level secret key.  Returns a
 * pointer to a static buffer of size SCRAM_DEFAULT_SALT_LEN.
 */
static char *
scram_mock_salt(const char *username)
{



















}
