   
                                              
   
                                                                
   
                           
   
#include "postgres_fe.h"
#include "mainloop.h"

#include "command.h"
#include "common.h"
#include "input.h"
#include "prompt.h"
#include "settings.h"

#include "common/logging.h"
#include "mb/pg_wchar.h"

                                           
const PsqlScanCallbacks psqlscan_callbacks = {
    psql_get_variable,
};

   
                                                   
                                    
   
                                                        
                                  
   
int
MainLoop(FILE *source)
{
  PsqlScanState scan_state;                                   
  ConditionalStack cond_stack;                             
  volatile PQExpBuffer query_buf;                                            
  volatile PQExpBuffer previous_buf;                                       
                                                                         
                                               
  PQExpBuffer history_buf;                                                         
                                                                        
  char *line;                                                   
  int added_nl_pos;
  bool success;
  bool line_saved_in_history;
  volatile int successResult = EXIT_SUCCESS;
  volatile backslashResult slashCmdStatus = PSQL_CMD_UNKNOWN;
  volatile promptStatus_t prompt_status = PROMPT_READY;
  volatile int count_eof = 0;
  volatile bool die_on_error = false;
  FILE *prev_cmd_source;
  bool prev_cmd_interactive;
  uint64 prev_lineno;

                                     
  prev_cmd_source = pset.cur_cmd_source;
  prev_cmd_interactive = pset.cur_cmd_interactive;
  prev_lineno = pset.lineno;
                                                               

                            
  pset.cur_cmd_source = source;
  pset.cur_cmd_interactive = ((source == stdin) && !pset.notty);
  pset.lineno = 0;
  pset.stmt_lineno = 1;

                            
  scan_state = psql_scan_create(&psqlscan_callbacks);
  cond_stack = conditional_stack_create();
  psql_scan_set_passthrough(scan_state, (void *)cond_stack);

  query_buf = createPQExpBuffer();
  previous_buf = createPQExpBuffer();
  history_buf = createPQExpBuffer();
  if (PQExpBufferBroken(query_buf) || PQExpBufferBroken(previous_buf) || PQExpBufferBroken(history_buf))
  {
    pg_log_error("out of memory");
    exit(EXIT_FAILURE);
  }

                                                 
  while (successResult == EXIT_SUCCESS)
  {
       
                                           
       
    if (cancel_pressed)
    {
      if (!pset.cur_cmd_interactive)
      {
           
                                                             
           
        successResult = EXIT_USER;
        break;
      }

      cancel_pressed = false;
    }

       
                                                                         
                                                                        
                                                          
       
    if (sigsetjmp(sigint_interrupt_jmp, 1) != 0)
    {
                                 

                               
      psql_scan_finish(scan_state);
      psql_scan_reset(scan_state);
      resetPQExpBuffer(query_buf);
      resetPQExpBuffer(history_buf);
      count_eof = 0;
      slashCmdStatus = PSQL_CMD_UNKNOWN;
      prompt_status = PROMPT_READY;
      pset.stmt_lineno = 1;
      cancel_pressed = false;

      if (pset.cur_cmd_interactive)
      {
        putc('\n', stdout);

           
                                                                    
                                        
           
        if (!conditional_stack_empty(cond_stack))
        {
          pg_log_error("\\if: escaped");
          conditional_stack_pop(cond_stack);
        }
      }
      else
      {
        successResult = EXIT_USER;
        break;
      }
    }

    fflush(stdout);

       
                        
       
    if (pset.cur_cmd_interactive)
    {
                                                         
      if (query_buf->len == 0)
      {
        prompt_status = PROMPT_READY;
      }
      line = gets_interactive(get_prompt(prompt_status, cond_stack), query_buf);
    }
    else
    {
      line = gets_fromFile(source);
      if (!line && ferror(source))
      {
        successResult = EXIT_FAILURE;
      }
    }

       
                                                                        
                                                                        
       

                                                  
    if (line == NULL)
    {
      if (pset.cur_cmd_interactive)
      {
                                                           
        count_eof++;

        if (count_eof < pset.ignoreeof)
        {
          if (!pset.quiet)
          {
            printf(_("Use \"\\q\" to leave %s.\n"), pset.progname);
          }
          continue;
        }

        puts(pset.quiet ? "" : "\\q");
      }
      break;
    }

    count_eof = 0;

    pset.lineno++;

                                              
    if (pset.lineno == 1 && pset.encoding == PG_UTF8 && strncmp(line, "\xef\xbb\xbf", 3) == 0)
    {
      memmove(line, line + 3, strlen(line + 3) + 1);
    }

                                                                   
    if (pset.lineno == 1 && !pset.cur_cmd_interactive && strncmp(line, "PGDMP", 5) == 0)
    {
      free(line);
      puts(_("The input is a PostgreSQL custom-format dump.\n"
             "Use the pg_restore command-line client to restore this dump to a database.\n"));
      fflush(stdout);
      successResult = EXIT_FAILURE;
      break;
    }

                                                                       
    if (line[0] == '\0' && !psql_scan_in_quote(scan_state))
    {
      free(line);
      continue;
    }

                                                                   
    if (pset.cur_cmd_interactive)
    {
      char *first_word = line;
      char *rest_of_line = NULL;
      bool found_help = false;
      bool found_exit_or_quit = false;
      bool found_q = false;

                                                                  
      if (pg_strncasecmp(first_word, "help", 4) == 0)
      {
        rest_of_line = first_word + 4;
        found_help = true;
      }
      else if (pg_strncasecmp(first_word, "exit", 4) == 0 || pg_strncasecmp(first_word, "quit", 4) == 0)
      {
        rest_of_line = first_word + 4;
        found_exit_or_quit = true;
      }

      else if (strncmp(first_word, "\\q", 2) == 0)
      {
        rest_of_line = first_word + 2;
        found_q = true;
      }

         
                                                                        
                                                                     
                                                                         
                                                                      
         
      if (rest_of_line != NULL)
      {
           
                                                                    
                     
           
        while (isspace((unsigned char)*rest_of_line))
        {
          ++rest_of_line;
        }
        if (*rest_of_line == ';')
        {
          ++rest_of_line;
        }
        while (isspace((unsigned char)*rest_of_line))
        {
          ++rest_of_line;
        }
        if (*rest_of_line != '\0')
        {
          found_help = false;
          found_exit_or_quit = false;
        }
      }

         
                                                                         
                                                                     
                                                                     
               
         
      if (found_help)
      {
        if (query_buf->len != 0)
#ifndef WIN32
          puts(_("Use \\? for help or press control-C to clear the input buffer."));
#else
          puts(_("Use \\? for help."));
#endif
        else
        {
          puts(_("You are using psql, the command-line interface to PostgreSQL."));
          printf(_("Type:  \\copyright for distribution terms\n"
                   "       \\h for help with SQL commands\n"
                   "       \\? for help with psql commands\n"
                   "       \\g or terminate with semicolon to execute query\n"
                   "       \\q to quit\n"));
          free(line);
          fflush(stdout);
          continue;
        }
      }

         
                                                                      
                                                                     
                                                                    
                              
         
      if (found_exit_or_quit)
      {
        if (query_buf->len != 0)
        {
          if (prompt_status == PROMPT_READY || prompt_status == PROMPT_CONTINUE || prompt_status == PROMPT_PAREN)
          {
            puts(_("Use \\q to quit."));
          }
          else
#ifndef WIN32
            puts(_("Use control-D to quit."));
#else
            puts(_("Use control-C to quit."));
#endif
        }
        else
        {
                        
          free(line);
          fflush(stdout);
          successResult = EXIT_SUCCESS;
          break;
        }
      }

         
                                                                        
                                                               
         
      if (found_q && query_buf->len != 0 && prompt_status != PROMPT_READY && prompt_status != PROMPT_CONTINUE && prompt_status != PROMPT_PAREN)
#ifndef WIN32
        puts(_("Use control-D to quit."));
#else
        puts(_("Use control-C to quit."));
#endif
    }

                                                      
    if (pset.echo == PSQL_ECHO_ALL && !pset.cur_cmd_interactive)
    {
      puts(line);
      fflush(stdout);
    }

                                                                
    if (query_buf->len > 0)
    {
      appendPQExpBufferChar(query_buf, '\n');
      added_nl_pos = query_buf->len;
    }
    else
    {
      added_nl_pos = -1;                             
    }

                                                            
    die_on_error = pset.on_error_stop;

       
                                                   
       
    psql_scan_setup(scan_state, line, strlen(line), pset.encoding, standard_strings());
    success = true;
    line_saved_in_history = false;

    while (success || !die_on_error)
    {
      PsqlScanResult scan_result;
      promptStatus_t prompt_tmp = prompt_status;
      size_t pos_in_query;
      char *tmp_line;

      pos_in_query = query_buf->len;
      scan_result = psql_scan(scan_state, query_buf, &prompt_tmp);
      prompt_status = prompt_tmp;

      if (PQExpBufferBroken(query_buf))
      {
        pg_log_error("out of memory");
        exit(EXIT_FAILURE);
      }

         
                                                                         
                                                                      
                                                               
                                                 
         
      tmp_line = query_buf->data + pos_in_query;
      while (*tmp_line != '\0')
      {
        if (*(tmp_line++) == '\n')
        {
          pset.stmt_lineno++;
        }
      }

      if (scan_result == PSCAN_EOL)
      {
        pset.stmt_lineno++;
      }

         
                                                                         
                           
         
      if (scan_result == PSCAN_SEMICOLON || (scan_result == PSCAN_EOL && pset.singleline))
      {
           
                                                                   
                                                                      
                                                                    
                                                                
           
        if (pset.cur_cmd_interactive && !line_saved_in_history)
        {
          pg_append_history(line, history_buf);
          pg_send_history(history_buf);
          line_saved_in_history = true;
        }

                                                                  
        if (conditional_active(cond_stack))
        {
          success = SendQuery(query_buf->data);
          slashCmdStatus = success ? PSQL_CMD_SEND : PSQL_CMD_ERROR;
          pset.stmt_lineno = 1;

                                                                  
          {
            PQExpBuffer swap_buf = previous_buf;

            previous_buf = query_buf;
            query_buf = swap_buf;
          }
          resetPQExpBuffer(query_buf);

          added_nl_pos = -1;
                                                     
        }
        else
        {
                                                             
          if (pset.cur_cmd_interactive)
          {
            pg_log_error("query ignored; use \\endif or Ctrl-C to exit current \\if block");
          }
                                                             
          success = true;
          slashCmdStatus = PSQL_CMD_SEND;
          pset.stmt_lineno = 1;
                                                        
        }
      }
      else if (scan_result == PSCAN_BACKSLASH)
      {
                                      

           
                                                                    
                                                                       
                                                                      
                                                                  
                                                                   
                                                                   
                                                     
           
        if (query_buf->len == added_nl_pos)
        {
          query_buf->data[--query_buf->len] = '\0';
          pg_send_history(history_buf);
        }
        added_nl_pos = -1;

                                               
        if (pset.cur_cmd_interactive && !line_saved_in_history)
        {
          pg_append_history(line, history_buf);
          pg_send_history(history_buf);
          line_saved_in_history = true;
        }

                                       
        slashCmdStatus = HandleSlashCmds(scan_state, cond_stack, query_buf, previous_buf);

        success = slashCmdStatus != PSQL_CMD_ERROR;

           
                                                                 
                                                                     
                                               
           
        pset.stmt_lineno = 1;

        if (slashCmdStatus == PSQL_CMD_SEND)
        {
                                                      
          Assert(conditional_active(cond_stack));

          success = SendQuery(query_buf->data);

                                                                  
          {
            PQExpBuffer swap_buf = previous_buf;

            previous_buf = query_buf;
            query_buf = swap_buf;
          }
          resetPQExpBuffer(query_buf);

                                                              
          psql_scan_reset(scan_state);
        }
        else if (slashCmdStatus == PSQL_CMD_NEWEDIT)
        {
                                                      
          Assert(conditional_active(cond_stack));
                                             
          psql_scan_finish(scan_state);
          free(line);
          line = pg_strdup(query_buf->data);
          resetPQExpBuffer(query_buf);
                                                                      
          psql_scan_reset(scan_state);
          psql_scan_setup(scan_state, line, strlen(line), pset.encoding, standard_strings());
          line_saved_in_history = false;
          prompt_status = PROMPT_READY;
        }
        else if (slashCmdStatus == PSQL_CMD_TERMINATE)
        {
          break;
        }
      }

                                                 
      if (scan_result == PSCAN_INCOMPLETE || scan_result == PSCAN_EOL)
      {
        break;
      }
    }

                                                                       
    if (pset.cur_cmd_interactive && !line_saved_in_history)
    {
      pg_append_history(line, history_buf);
    }

    psql_scan_finish(scan_state);
    free(line);

    if (slashCmdStatus == PSQL_CMD_TERMINATE)
    {
      successResult = EXIT_SUCCESS;
      break;
    }

    if (!pset.cur_cmd_interactive)
    {
      if (!success && die_on_error)
      {
        successResult = EXIT_USER;
      }
                                           
      else if (!pset.db)
      {
        successResult = EXIT_BADCONN;
      }
    }
  }                               

     
                                                                        
                                                                           
                                                                             
     
  if (query_buf->len > 0 && !pset.cur_cmd_interactive && successResult == EXIT_SUCCESS)
  {
                               
                                                                         
#ifdef NOT_USED
    if (pset.cur_cmd_interactive)
    {
      pg_send_history(history_buf);
    }
#endif

                                                              
    if (conditional_active(cond_stack))
    {
      success = SendQuery(query_buf->data);
    }
    else
    {
      if (pset.cur_cmd_interactive)
      {
        pg_log_error("query ignored; use \\endif or Ctrl-C to exit current \\if block");
      }
      success = true;
    }

    if (!success && die_on_error)
    {
      successResult = EXIT_USER;
    }
    else if (pset.db == NULL)
    {
      successResult = EXIT_BADCONN;
    }
  }

     
                                                                          
                            
     
  if (slashCmdStatus != PSQL_CMD_TERMINATE && successResult != EXIT_USER && !conditional_stack_empty(cond_stack))
  {
    pg_log_error("reached EOF without finding closing \\endif(s)");
    if (die_on_error && !pset.cur_cmd_interactive)
    {
      successResult = EXIT_USER;
    }
  }

     
                                                                   
                                                                            
                                                                       
                                                                         
              
     
  sigint_interrupt_enabled = false;

  destroyPQExpBuffer(query_buf);
  destroyPQExpBuffer(previous_buf);
  destroyPQExpBuffer(history_buf);

  psql_scan_destroy(scan_state);
  conditional_stack_destroy(cond_stack);

  pset.cur_cmd_source = prev_cmd_source;
  pset.cur_cmd_interactive = prev_cmd_interactive;
  pset.lineno = prev_lineno;

  return successResult;
}                 
