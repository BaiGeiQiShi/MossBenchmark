   
                                                            
   
                                   
   
                              
   

#include "postgres.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "access/stratnum.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "common/ip.h"
#include "libpq/libpq-be.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/hashutils.h"
#include "utils/inet.h"
#include "utils/lsyscache.h"

static int32
network_cmp_internal(inet *a1, inet *a2);
static List *
match_network_function(Node *leftop, Node *rightop, int indexarg, Oid funcid, Oid opfamily);
static List *
match_network_subset(Node *leftop, Node *rightop, bool is_eq, Oid opfamily);
static bool
addressOK(unsigned char *a, int bits, int family);
static inet *
internal_inetpl(inet *ip, int64 addend);

   
                                  
   
static inet *
network_in(char *src, bool is_cidr)
{
  int bits;
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

     
                                                                             
                                                                           
                                                        
     

  if (strchr(src, ':') != NULL)
  {
    ip_family(dst) = PGSQL_AF_INET6;
  }
  else
  {
    ip_family(dst) = PGSQL_AF_INET;
  }

  bits = inet_net_pton(ip_family(dst), src, ip_addr(dst), is_cidr ? ip_addrsize(dst) : -1);
  if ((bits < 0) || (bits > ip_maxbits(dst)))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                                                                 
                       errmsg("invalid input syntax for type %s: \"%s\"", is_cidr ? "cidr" : "inet", src)));
  }

     
                                                                             
     
  if (is_cidr)
  {
    if (!addressOK(ip_addr(dst), bits, ip_family(dst)))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid cidr value: \"%s\"", src), errdetail("Value has bits set to right of mask.")));
    }
  }

  ip_bits(dst) = bits;
  SET_INET_VARSIZE(dst);

  return dst;
}

Datum
inet_in(PG_FUNCTION_ARGS)
{
  char *src = PG_GETARG_CSTRING(0);

  PG_RETURN_INET_P(network_in(src, false));
}

Datum
cidr_in(PG_FUNCTION_ARGS)
{
  char *src = PG_GETARG_CSTRING(0);

  PG_RETURN_INET_P(network_in(src, true));
}

   
                                   
   
static char *
network_out(inet *src, bool is_cidr)
{
  char tmp[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128")];
  char *dst;
  int len;

  dst = inet_net_ntop(ip_family(src), ip_addr(src), ip_bits(src), tmp, sizeof(tmp));
  if (dst == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("could not format inet value: %m")));
  }

                                       
  if (is_cidr && strchr(tmp, '/') == NULL)
  {
    len = strlen(tmp);
    snprintf(tmp + len, sizeof(tmp) - len, "/%u", ip_bits(src));
  }

  return pstrdup(tmp);
}

Datum
inet_out(PG_FUNCTION_ARGS)
{
  inet *src = PG_GETARG_INET_PP(0);

  PG_RETURN_CSTRING(network_out(src, false));
}

Datum
cidr_out(PG_FUNCTION_ARGS)
{
  inet *src = PG_GETARG_INET_PP(0);

  PG_RETURN_CSTRING(network_out(src, true));
}

   
                                                            
   
                                                        
                                                                         
   
                                                                          
                                                                        
                                          
   
static inet *
network_recv(StringInfo buf, bool is_cidr)
{
  inet *addr;
  char *addrptr;
  int bits;
  int nb, i;

                                                            
  addr = (inet *)palloc0(sizeof(inet));

  ip_family(addr) = pq_getmsgbyte(buf);
  if (ip_family(addr) != PGSQL_AF_INET && ip_family(addr) != PGSQL_AF_INET6)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                                                           
                       errmsg("invalid address family in external \"%s\" value", is_cidr ? "cidr" : "inet")));
  }
  bits = pq_getmsgbyte(buf);
  if (bits < 0 || bits > ip_maxbits(addr))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                                                           
                       errmsg("invalid bits in external \"%s\" value", is_cidr ? "cidr" : "inet")));
  }
  ip_bits(addr) = bits;
  i = pq_getmsgbyte(buf);                     
  nb = pq_getmsgbyte(buf);
  if (nb != ip_addrsize(addr))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION),
                                                           
                       errmsg("invalid length in external \"%s\" value", is_cidr ? "cidr" : "inet")));
  }

  addrptr = (char *)ip_addr(addr);
  for (i = 0; i < nb; i++)
  {
    addrptr[i] = pq_getmsgbyte(buf);
  }

     
                                                                             
     
  if (is_cidr)
  {
    if (!addressOK(ip_addr(addr), bits, ip_family(addr)))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid external \"cidr\" value"), errdetail("Value has bits set to right of mask.")));
    }
  }

  SET_INET_VARSIZE(addr);

  return addr;
}

Datum
inet_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_INET_P(network_recv(buf, false));
}

Datum
cidr_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

  PG_RETURN_INET_P(network_recv(buf, true));
}

   
                                                   
   
static bytea *
network_send(inet *addr, bool is_cidr)
{
  StringInfoData buf;
  char *addrptr;
  int nb, i;

  pq_begintypsend(&buf);
  pq_sendbyte(&buf, ip_family(addr));
  pq_sendbyte(&buf, ip_bits(addr));
  pq_sendbyte(&buf, is_cidr);
  nb = ip_addrsize(addr);
  if (nb < 0)
  {
    nb = 0;
  }
  pq_sendbyte(&buf, nb);
  addrptr = (char *)ip_addr(addr);
  for (i = 0; i < nb; i++)
  {
    pq_sendbyte(&buf, addrptr[i]);
  }
  return pq_endtypsend(&buf);
}

Datum
inet_send(PG_FUNCTION_ARGS)
{
  inet *addr = PG_GETARG_INET_PP(0);

  PG_RETURN_BYTEA_P(network_send(addr, false));
}

Datum
cidr_send(PG_FUNCTION_ARGS)
{
  inet *addr = PG_GETARG_INET_PP(0);

  PG_RETURN_BYTEA_P(network_send(addr, true));
}

Datum
inet_to_cidr(PG_FUNCTION_ARGS)
{
  inet *src = PG_GETARG_INET_PP(0);
  int bits;

  bits = ip_bits(src);

                    
  if ((bits < 0) || (bits > ip_maxbits(src)))
  {
    elog(ERROR, "invalid inet bit length: %d", bits);
  }

  PG_RETURN_INET_P(cidr_set_masklen_internal(src, bits));
}

Datum
inet_set_masklen(PG_FUNCTION_ARGS)
{
  inet *src = PG_GETARG_INET_PP(0);
  int bits = PG_GETARG_INT32(1);
  inet *dst;

  if (bits == -1)
  {
    bits = ip_maxbits(src);
  }

  if ((bits < 0) || (bits > ip_maxbits(src)))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid mask length: %d", bits)));
  }

                               
  dst = (inet *)palloc(VARSIZE_ANY(src));
  memcpy(dst, src, VARSIZE_ANY(src));

  ip_bits(dst) = bits;

  PG_RETURN_INET_P(dst);
}

Datum
cidr_set_masklen(PG_FUNCTION_ARGS)
{
  inet *src = PG_GETARG_INET_PP(0);
  int bits = PG_GETARG_INT32(1);

  if (bits == -1)
  {
    bits = ip_maxbits(src);
  }

  if ((bits < 0) || (bits > ip_maxbits(src)))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid mask length: %d", bits)));
  }

  PG_RETURN_INET_P(cidr_set_masklen_internal(src, bits));
}

   
                                                                               
   
inet *
cidr_set_masklen_internal(const inet *src, int bits)
{
  inet *dst = (inet *)palloc0(sizeof(inet));

  ip_family(dst) = ip_family(src);
  ip_bits(dst) = bits;

  if (bits > 0)
  {
    Assert(bits <= ip_maxbits(dst));

                                                                    
    memcpy(ip_addr(dst), ip_addr(src), (bits + 7) / 8);

                                                          
    if (bits % 8)
    {
      ip_addr(dst)[bits / 8] &= ~(0xFF >> (bits % 8));
    }
  }

                                    
  SET_INET_VARSIZE(dst);

  return dst;
}

   
                                                                    
   
                                                                       
                                                                           
                                                                      
                                                                         
                                                                          
                                                            
   

static int32
network_cmp_internal(inet *a1, inet *a2)
{
  if (ip_family(a1) == ip_family(a2))
  {
    int order;

    order = bitncmp(ip_addr(a1), ip_addr(a2), Min(ip_bits(a1), ip_bits(a2)));
    if (order != 0)
    {
      return order;
    }
    order = ((int)ip_bits(a1)) - ((int)ip_bits(a2));
    if (order != 0)
    {
      return order;
    }
    return bitncmp(ip_addr(a1), ip_addr(a2), ip_maxbits(a1));
  }

  return ip_family(a1) - ip_family(a2);
}

Datum
network_cmp(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_INT32(network_cmp_internal(a1, a2));
}

   
                           
   
Datum
network_lt(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(network_cmp_internal(a1, a2) < 0);
}

Datum
network_le(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(network_cmp_internal(a1, a2) <= 0);
}

Datum
network_eq(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(network_cmp_internal(a1, a2) == 0);
}

Datum
network_ge(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(network_cmp_internal(a1, a2) >= 0);
}

Datum
network_gt(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(network_cmp_internal(a1, a2) > 0);
}

Datum
network_ne(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(network_cmp_internal(a1, a2) != 0);
}

   
                              
   
Datum
network_smaller(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (network_cmp_internal(a1, a2) < 0)
  {
    PG_RETURN_INET_P(a1);
  }
  else
  {
    PG_RETURN_INET_P(a2);
  }
}

Datum
network_larger(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (network_cmp_internal(a1, a2) > 0)
  {
    PG_RETURN_INET_P(a1);
  }
  else
  {
    PG_RETURN_INET_P(a2);
  }
}

   
                                                   
   
Datum
hashinet(PG_FUNCTION_ARGS)
{
  inet *addr = PG_GETARG_INET_PP(0);
  int addrsize = ip_addrsize(addr);

                                                                     
  return hash_any((unsigned char *)VARDATA_ANY(addr), addrsize + 2);
}

Datum
hashinetextended(PG_FUNCTION_ARGS)
{
  inet *addr = PG_GETARG_INET_PP(0);
  int addrsize = ip_addrsize(addr);

  return hash_any_extended((unsigned char *)VARDATA_ANY(addr), addrsize + 2, PG_GETARG_INT64(1));
}

   
                                    
   
Datum
network_sub(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (ip_family(a1) == ip_family(a2))
  {
    PG_RETURN_BOOL(ip_bits(a1) > ip_bits(a2) && bitncmp(ip_addr(a1), ip_addr(a2), ip_bits(a2)) == 0);
  }

  PG_RETURN_BOOL(false);
}

Datum
network_subeq(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (ip_family(a1) == ip_family(a2))
  {
    PG_RETURN_BOOL(ip_bits(a1) >= ip_bits(a2) && bitncmp(ip_addr(a1), ip_addr(a2), ip_bits(a2)) == 0);
  }

  PG_RETURN_BOOL(false);
}

Datum
network_sup(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (ip_family(a1) == ip_family(a2))
  {
    PG_RETURN_BOOL(ip_bits(a1) < ip_bits(a2) && bitncmp(ip_addr(a1), ip_addr(a2), ip_bits(a1)) == 0);
  }

  PG_RETURN_BOOL(false);
}

Datum
network_supeq(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (ip_family(a1) == ip_family(a2))
  {
    PG_RETURN_BOOL(ip_bits(a1) <= ip_bits(a2) && bitncmp(ip_addr(a1), ip_addr(a2), ip_bits(a1)) == 0);
  }

  PG_RETURN_BOOL(false);
}

Datum
network_overlap(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  if (ip_family(a1) == ip_family(a2))
  {
    PG_RETURN_BOOL(bitncmp(ip_addr(a1), ip_addr(a2), Min(ip_bits(a1), ip_bits(a2))) == 0);
  }

  PG_RETURN_BOOL(false);
}

   
                                                                  
   
Datum
network_subset_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestIndexCondition))
  {
                                                                   
    SupportRequestIndexCondition *req = (SupportRequestIndexCondition *)rawreq;

    if (is_opclause(req->node))
    {
      OpExpr *clause = (OpExpr *)req->node;

      Assert(list_length(clause->args) == 2);
      ret = (Node *)match_network_function((Node *)linitial(clause->args), (Node *)lsecond(clause->args), req->indexarg, req->funcid, req->opfamily);
    }
    else if (is_funcclause(req->node))                  
    {
      FuncExpr *clause = (FuncExpr *)req->node;

      Assert(list_length(clause->args) == 2);
      ret = (Node *)match_network_function((Node *)linitial(clause->args), (Node *)lsecond(clause->args), req->indexarg, req->funcid, req->opfamily);
    }
  }

  PG_RETURN_POINTER(ret);
}

   
                          
                                                                          
   
                                                                           
                               
   
static List *
match_network_function(Node *leftop, Node *rightop, int indexarg, Oid funcid, Oid opfamily)
{
  switch (funcid)
  {
  case F_NETWORK_SUB:
                                      
    if (indexarg != 0)
    {
      return NIL;
    }
    return match_network_subset(leftop, rightop, false, opfamily);

  case F_NETWORK_SUBEQ:
                                      
    if (indexarg != 0)
    {
      return NIL;
    }
    return match_network_subset(leftop, rightop, true, opfamily);

  case F_NETWORK_SUP:
                                       
    if (indexarg != 1)
    {
      return NIL;
    }
    return match_network_subset(rightop, leftop, false, opfamily);

  case F_NETWORK_SUPEQ:
                                       
    if (indexarg != 1)
    {
      return NIL;
    }
    return match_network_subset(rightop, leftop, true, opfamily);

  default:

       
                                                                     
                                                                     
                        
       
    return NIL;
  }
}

   
                        
                                                                 
   
static List *
match_network_subset(Node *leftop, Node *rightop, bool is_eq, Oid opfamily)
{
  List *result;
  Datum rightopval;
  Oid datatype = INETOID;
  Oid opr1oid;
  Oid opr2oid;
  Datum opr1right;
  Datum opr2right;
  Expr *expr;

     
                                                                     
     
                                                                            
                                                                           
                           
     
  if (!IsA(rightop, Const) || ((Const *)rightop)->constisnull)
  {
    return NIL;
  }
  rightopval = ((Const *)rightop)->constvalue;

     
                                                                             
            
     
                                                                           
                                                                             
                         
     
  if (opfamily != NETWORK_BTREE_FAM_OID)
  {
    return NIL;
  }

     
                                                                            
                                  
     
                                                                             
                                                                        
                                                                  
     
  if (is_eq)
  {
    opr1oid = get_opfamily_member(opfamily, datatype, datatype, BTGreaterEqualStrategyNumber);
    if (opr1oid == InvalidOid)
    {
      elog(ERROR, "no >= operator for opfamily %u", opfamily);
    }
  }
  else
  {
    opr1oid = get_opfamily_member(opfamily, datatype, datatype, BTGreaterStrategyNumber);
    if (opr1oid == InvalidOid)
    {
      elog(ERROR, "no > operator for opfamily %u", opfamily);
    }
  }

  opr1right = network_scan_first(rightopval);

  expr = make_opclause(opr1oid, BOOLOID, false, (Expr *)leftop,
      (Expr *)makeConst(datatype, -1, InvalidOid,                     
          -1, opr1right, false, false),
      InvalidOid, InvalidOid);
  result = list_make1(expr);

                                                              

  opr2oid = get_opfamily_member(opfamily, datatype, datatype, BTLessEqualStrategyNumber);
  if (opr2oid == InvalidOid)
  {
    elog(ERROR, "no <= operator for opfamily %u", opfamily);
  }

  opr2right = network_scan_last(rightopval);

  expr = make_opclause(opr2oid, BOOLOID, false, (Expr *)leftop,
      (Expr *)makeConst(datatype, -1, InvalidOid,                     
          -1, opr2right, false, false),
      InvalidOid, InvalidOid);
  result = lappend(result, expr);

  return result;
}

   
                                         
   
Datum
network_host(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  char *ptr;
  char tmp[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128")];

                                                           
  if (inet_net_ntop(ip_family(ip), ip_addr(ip), ip_maxbits(ip), tmp, sizeof(tmp)) == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("could not format inet value: %m")));
  }

                                                     
  if ((ptr = strchr(tmp, '/')) != NULL)
  {
    *ptr = '\0';
  }

  PG_RETURN_TEXT_P(cstring_to_text(tmp));
}

   
                                                                         
                                                                           
                   
   
Datum
network_show(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  int len;
  char tmp[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128")];

  if (inet_net_ntop(ip_family(ip), ip_addr(ip), ip_maxbits(ip), tmp, sizeof(tmp)) == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("could not format inet value: %m")));
  }

                                                 
  if (strchr(tmp, '/') == NULL)
  {
    len = strlen(tmp);
    snprintf(tmp + len, sizeof(tmp) - len, "/%u", ip_bits(ip));
  }

  PG_RETURN_TEXT_P(cstring_to_text(tmp));
}

Datum
inet_abbrev(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  char *dst;
  char tmp[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128")];

  dst = inet_net_ntop(ip_family(ip), ip_addr(ip), ip_bits(ip), tmp, sizeof(tmp));

  if (dst == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("could not format inet value: %m")));
  }

  PG_RETURN_TEXT_P(cstring_to_text(tmp));
}

Datum
cidr_abbrev(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  char *dst;
  char tmp[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255/128")];

  dst = inet_cidr_ntop(ip_family(ip), ip_addr(ip), ip_bits(ip), tmp, sizeof(tmp));

  if (dst == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("could not format cidr value: %m")));
  }

  PG_RETURN_TEXT_P(cstring_to_text(tmp));
}

Datum
network_masklen(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);

  PG_RETURN_INT32(ip_bits(ip));
}

Datum
network_family(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);

  switch (ip_family(ip))
  {
  case PGSQL_AF_INET:
    PG_RETURN_INT32(4);
    break;
  case PGSQL_AF_INET6:
    PG_RETURN_INT32(6);
    break;
  default:
    PG_RETURN_INT32(0);
    break;
  }
}

Datum
network_broadcast(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *dst;
  int byte;
  int bits;
  int maxbytes;
  unsigned char mask;
  unsigned char *a, *b;

                                            
  dst = (inet *)palloc0(sizeof(inet));

  maxbytes = ip_addrsize(ip);
  bits = ip_bits(ip);
  a = ip_addr(ip);
  b = ip_addr(dst);

  for (byte = 0; byte < maxbytes; byte++)
  {
    if (bits >= 8)
    {
      mask = 0x00;
      bits -= 8;
    }
    else if (bits == 0)
    {
      mask = 0xff;
    }
    else
    {
      mask = 0xff >> bits;
      bits = 0;
    }

    b[byte] = a[byte] | mask;
  }

  ip_family(dst) = ip_family(ip);
  ip_bits(dst) = ip_bits(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

Datum
network_network(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *dst;
  int byte;
  int bits;
  unsigned char mask;
  unsigned char *a, *b;

                                            
  dst = (inet *)palloc0(sizeof(inet));

  bits = ip_bits(ip);
  a = ip_addr(ip);
  b = ip_addr(dst);

  byte = 0;

  while (bits)
  {
    if (bits >= 8)
    {
      mask = 0xff;
      bits -= 8;
    }
    else
    {
      mask = 0xff << (8 - bits);
      bits = 0;
    }

    b[byte] = a[byte] & mask;
    byte++;
  }

  ip_family(dst) = ip_family(ip);
  ip_bits(dst) = ip_bits(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

Datum
network_netmask(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *dst;
  int byte;
  int bits;
  unsigned char mask;
  unsigned char *b;

                                            
  dst = (inet *)palloc0(sizeof(inet));

  bits = ip_bits(ip);
  b = ip_addr(dst);

  byte = 0;

  while (bits)
  {
    if (bits >= 8)
    {
      mask = 0xff;
      bits -= 8;
    }
    else
    {
      mask = 0xff << (8 - bits);
      bits = 0;
    }

    b[byte] = mask;
    byte++;
  }

  ip_family(dst) = ip_family(ip);
  ip_bits(dst) = ip_maxbits(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

Datum
network_hostmask(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *dst;
  int byte;
  int bits;
  int maxbytes;
  unsigned char mask;
  unsigned char *b;

                                            
  dst = (inet *)palloc0(sizeof(inet));

  maxbytes = ip_addrsize(ip);
  bits = ip_maxbits(ip) - ip_bits(ip);
  b = ip_addr(dst);

  byte = maxbytes - 1;

  while (bits)
  {
    if (bits >= 8)
    {
      mask = 0xff;
      bits -= 8;
    }
    else
    {
      mask = 0xff >> (8 - bits);
      bits = 0;
    }

    b[byte] = mask;
    byte--;
  }

  ip_family(dst) = ip_family(ip);
  ip_bits(dst) = ip_maxbits(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

   
                                                                              
                                                                           
   
Datum
inet_same_family(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0);
  inet *a2 = PG_GETARG_INET_PP(1);

  PG_RETURN_BOOL(ip_family(a1) == ip_family(a2));
}

   
                                                                
   
Datum
inet_merge(PG_FUNCTION_ARGS)
{
  inet *a1 = PG_GETARG_INET_PP(0), *a2 = PG_GETARG_INET_PP(1);
  int commonbits;

  if (ip_family(a1) != ip_family(a2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot merge addresses from different families")));
  }

  commonbits = bitncommon(ip_addr(a1), ip_addr(a2), Min(ip_bits(a1), ip_bits(a2)));

  PG_RETURN_INET_P(cidr_set_masklen_internal(a1, commonbits));
}

   
                                                                         
                                                                     
                            
   
                                                               
                                            
   
double
convert_network_to_scalar(Datum value, Oid typid, bool *failure)
{
  switch (typid)
  {
  case INETOID:
  case CIDROID:
  {
    inet *ip = DatumGetInetPP(value);
    int len;
    double res;
    int i;

       
                                                         
       
    if (ip_family(ip) == PGSQL_AF_INET)
    {
      len = 4;
    }
    else
    {
      len = 5;
    }

    res = ip_family(ip);
    for (i = 0; i < len; i++)
    {
      res *= 256;
      res += ip_addr(ip)[i];
    }
    return res;
  }
  case MACADDROID:
  {
    macaddr *mac = DatumGetMacaddrP(value);
    double res;

    res = (mac->a << 16) | (mac->b << 8) | (mac->c);
    res *= 256 * 256 * 256;
    res += (mac->d << 16) | (mac->e << 8) | (mac->f);
    return res;
  }
  case MACADDR8OID:
  {
    macaddr8 *mac = DatumGetMacaddr8P(value);
    double res;

    res = (mac->a << 24) | (mac->b << 16) | (mac->c << 8) | (mac->d);
    res *= ((double)256) * 256 * 256 * 256;
    res += (mac->e << 24) | (mac->f << 16) | (mac->g << 8) | (mac->h);
    return res;
  }
  }

  *failure = true;
  return 0;
}

   
       
                    
                                           
           
                                        
         
                                                               
                                    
           
                                
   
int
bitncmp(const unsigned char *l, const unsigned char *r, int n)
{
  unsigned int lb, rb;
  int x, b;

  b = n / 8;
  x = memcmp(l, r, b);
  if (x || (n % 8) == 0)
  {
    return x;
  }

  lb = l[b];
  rb = r[b];
  for (b = n % 8; b > 0; b--)
  {
    if (IS_HIGHBIT_SET(lb) != IS_HIGHBIT_SET(rb))
    {
      if (IS_HIGHBIT_SET(lb))
      {
        return 1;
      }
      return -1;
    }
    lb <<= 1;
    rb <<= 1;
  }
  return 0;
}

   
                                                            
   
                                                           
   
int
bitncommon(const unsigned char *l, const unsigned char *r, int n)
{
  int byte, nbits;

                                              
  nbits = n % 8;

                         
  for (byte = 0; byte < n / 8; byte++)
  {
    if (l[byte] != r[byte])
    {
                                                           
      nbits = 7;
      break;
    }
  }

                                       
  if (nbits != 0)
  {
                                                    
    unsigned int diff = l[byte] ^ r[byte];

                                                     
    while ((diff >> (8 - nbits)) != 0)
    {
      nbits--;
    }
  }

  return (8 * byte) + nbits;
}

   
                                                                        
   
static bool
addressOK(unsigned char *a, int bits, int family)
{
  int byte;
  int nbits;
  int maxbits;
  int maxbytes;
  unsigned char mask;

  if (family == PGSQL_AF_INET)
  {
    maxbits = 32;
    maxbytes = 4;
  }
  else
  {
    maxbits = 128;
    maxbytes = 16;
  }
  Assert(bits <= maxbits);

  if (bits == maxbits)
  {
    return true;
  }

  byte = bits / 8;

  nbits = bits % 8;
  mask = 0xff;
  if (bits != 0)
  {
    mask >>= nbits;
  }

  while (byte < maxbytes)
  {
    if ((a[byte] & mask) != 0)
    {
      return false;
    }
    mask = 0xff;
    byte++;
  }

  return true;
}

   
                                                                    
                                  
   

                                                           
Datum
network_scan_first(Datum in)
{
  return DirectFunctionCall1(network_network, in);
}

   
                                                                    
                                                         
                                                             
   
                                                                       
                                                
   
Datum
network_scan_last(Datum in)
{
  return DirectFunctionCall2(inet_set_masklen, DirectFunctionCall1(network_broadcast, in), Int32GetDatum(-1));
}

   
                                                                       
   
Datum
inet_client_addr(PG_FUNCTION_ARGS)
{
  Port *port = MyProcPort;
  char remote_host[NI_MAXHOST];
  int ret;

  if (port == NULL)
  {
    PG_RETURN_NULL();
  }

  switch (port->raddr.addr.ss_family)
  {
  case AF_INET:
#ifdef HAVE_IPV6
  case AF_INET6:
#endif
    break;
  default:
    PG_RETURN_NULL();
  }

  remote_host[0] = '\0';

  ret = pg_getnameinfo_all(&port->raddr.addr, port->raddr.salen, remote_host, sizeof(remote_host), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0)
  {
    PG_RETURN_NULL();
  }

  clean_ipv6_addr(port->raddr.addr.ss_family, remote_host);

  PG_RETURN_INET_P(network_in(remote_host, false));
}

   
                                                                 
   
Datum
inet_client_port(PG_FUNCTION_ARGS)
{
  Port *port = MyProcPort;
  char remote_port[NI_MAXSERV];
  int ret;

  if (port == NULL)
  {
    PG_RETURN_NULL();
  }

  switch (port->raddr.addr.ss_family)
  {
  case AF_INET:
#ifdef HAVE_IPV6
  case AF_INET6:
#endif
    break;
  default:
    PG_RETURN_NULL();
  }

  remote_port[0] = '\0';

  ret = pg_getnameinfo_all(&port->raddr.addr, port->raddr.salen, NULL, 0, remote_port, sizeof(remote_port), NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(DirectFunctionCall1(int4in, CStringGetDatum(remote_port)));
}

   
                                                                               
   
Datum
inet_server_addr(PG_FUNCTION_ARGS)
{
  Port *port = MyProcPort;
  char local_host[NI_MAXHOST];
  int ret;

  if (port == NULL)
  {
    PG_RETURN_NULL();
  }

  switch (port->laddr.addr.ss_family)
  {
  case AF_INET:
#ifdef HAVE_IPV6
  case AF_INET6:
#endif
    break;
  default:
    PG_RETURN_NULL();
  }

  local_host[0] = '\0';

  ret = pg_getnameinfo_all(&port->laddr.addr, port->laddr.salen, local_host, sizeof(local_host), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0)
  {
    PG_RETURN_NULL();
  }

  clean_ipv6_addr(port->laddr.addr.ss_family, local_host);

  PG_RETURN_INET_P(network_in(local_host, false));
}

   
                                                                         
   
Datum
inet_server_port(PG_FUNCTION_ARGS)
{
  Port *port = MyProcPort;
  char local_port[NI_MAXSERV];
  int ret;

  if (port == NULL)
  {
    PG_RETURN_NULL();
  }

  switch (port->laddr.addr.ss_family)
  {
  case AF_INET:
#ifdef HAVE_IPV6
  case AF_INET6:
#endif
    break;
  default:
    PG_RETURN_NULL();
  }

  local_port[0] = '\0';

  ret = pg_getnameinfo_all(&port->laddr.addr, port->laddr.salen, NULL, 0, local_port, sizeof(local_port), NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(DirectFunctionCall1(int4in, CStringGetDatum(local_port)));
}

Datum
inetnot(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

  {
    int nb = ip_addrsize(ip);
    unsigned char *pip = ip_addr(ip);
    unsigned char *pdst = ip_addr(dst);

    while (nb-- > 0)
    {
      pdst[nb] = ~pip[nb];
    }
  }
  ip_bits(dst) = ip_bits(ip);

  ip_family(dst) = ip_family(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

Datum
inetand(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *ip2 = PG_GETARG_INET_PP(1);
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

  if (ip_family(ip) != ip_family(ip2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot AND inet values of different sizes")));
  }
  else
  {
    int nb = ip_addrsize(ip);
    unsigned char *pip = ip_addr(ip);
    unsigned char *pip2 = ip_addr(ip2);
    unsigned char *pdst = ip_addr(dst);

    while (nb-- > 0)
    {
      pdst[nb] = pip[nb] & pip2[nb];
    }
  }
  ip_bits(dst) = Max(ip_bits(ip), ip_bits(ip2));

  ip_family(dst) = ip_family(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

Datum
inetor(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *ip2 = PG_GETARG_INET_PP(1);
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

  if (ip_family(ip) != ip_family(ip2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot OR inet values of different sizes")));
  }
  else
  {
    int nb = ip_addrsize(ip);
    unsigned char *pip = ip_addr(ip);
    unsigned char *pip2 = ip_addr(ip2);
    unsigned char *pdst = ip_addr(dst);

    while (nb-- > 0)
    {
      pdst[nb] = pip[nb] | pip2[nb];
    }
  }
  ip_bits(dst) = Max(ip_bits(ip), ip_bits(ip2));

  ip_family(dst) = ip_family(ip);
  SET_INET_VARSIZE(dst);

  PG_RETURN_INET_P(dst);
}

static inet *
internal_inetpl(inet *ip, int64 addend)
{
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

  {
    int nb = ip_addrsize(ip);
    unsigned char *pip = ip_addr(ip);
    unsigned char *pdst = ip_addr(dst);
    int carry = 0;

    while (nb-- > 0)
    {
      carry = pip[nb] + (int)(addend & 0xFF) + carry;
      pdst[nb] = (unsigned char)(carry & 0xFF);
      carry >>= 8;

         
                                                                   
                                                                      
                                                                       
                                                                        
                                                                        
                                                                  
                                                      
         
      addend &= ~((int64)0xFF);
      addend /= 0x100;
    }

       
                                                                           
                                                                          
                                         
       
    if (!((addend == 0 && carry == 0) || (addend == -1 && carry == 1)))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("result is out of range")));
    }
  }

  ip_bits(dst) = ip_bits(ip);
  ip_family(dst) = ip_family(ip);
  SET_INET_VARSIZE(dst);

  return dst;
}

Datum
inetpl(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  int64 addend = PG_GETARG_INT64(1);

  PG_RETURN_INET_P(internal_inetpl(ip, addend));
}

Datum
inetmi_int8(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  int64 addend = PG_GETARG_INT64(1);

  PG_RETURN_INET_P(internal_inetpl(ip, -addend));
}

Datum
inetmi(PG_FUNCTION_ARGS)
{
  inet *ip = PG_GETARG_INET_PP(0);
  inet *ip2 = PG_GETARG_INET_PP(1);
  int64 res = 0;

  if (ip_family(ip) != ip_family(ip2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot subtract inet values of different sizes")));
  }
  else
  {
       
                                                                           
                                                                           
                                                                         
                                  
       
    int nb = ip_addrsize(ip);
    int byte = 0;
    unsigned char *pip = ip_addr(ip);
    unsigned char *pip2 = ip_addr(ip2);
    int carry = 1;

    while (nb-- > 0)
    {
      int lobyte;

      carry = pip[nb] + (~pip2[nb] & 0xFF) + carry;
      lobyte = carry & 0xFF;
      if (byte < sizeof(int64))
      {
        res |= ((int64)lobyte) << (byte * 8);
      }
      else
      {
           
                                                                     
                                                                       
                                            
           
        if ((res < 0) ? (lobyte != 0xFF) : (lobyte != 0))
        {
          ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("result is out of range")));
        }
      }
      carry >>= 8;
      byte++;
    }

       
                                                                         
                                         
       
    if (carry == 0 && byte < sizeof(int64))
    {
      res |= ((uint64)(int64)-1) << (byte * 8);
    }
  }

  PG_RETURN_INT64(res);
}

   
                                                                           
   
                                    
   
                                                                            
                                                                         
                                                                           
                                        
   
                                                                         
                                                                            
                                     
   
void
clean_ipv6_addr(int addr_family, char *addr)
{
#ifdef HAVE_IPV6
  if (addr_family == AF_INET6)
  {
    char *pct = strchr(addr, '%');

    if (pct)
    {
      *pct = '\0';
    }
  }
#endif
}
