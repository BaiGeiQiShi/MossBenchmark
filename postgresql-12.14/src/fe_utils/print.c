                                                                            
   
                                                   
   
                                                                          
                                                                       
                                                                     
                                                            
   
   
                                                                         
                                                                        
   
                        
   
                                                                            
   
#include "postgres_fe.h"

#include <limits.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

#ifndef WIN32
#include <sys/ioctl.h>                  
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#include "fe_utils/print.h"

#include "catalog/pg_type_d.h"
#include "fe_utils/mbprint.h"

   
                                                                 
                                           
   
                                                                              
                                           
   
volatile bool cancel_pressed = false;

static bool always_ignore_sigpipe = false;

                                                                            
static char *decimal_point;
static int groupdigits;
static char *thousands_sep;

static char default_footer[100];
static printTableFooter default_footer_cell = {default_footer, NULL};

                                   
const printTextFormat pg_asciiformat = {"ascii", {{"-", "+", "+", "+"}, {"-", "+", "+", "+"}, {"-", "+", "+", "+"}, {"", "|", "|", "|"}}, "|", "|", "|", " ", "+", " ", "+", ".", ".", true};

const printTextFormat pg_asciiformat_old = {"old-ascii", {{"-", "+", "+", "+"}, {"-", "+", "+", "+"}, {"-", "+", "+", "+"}, {"", "|", "|", "|"}}, ":", ";", " ", "+", " ", " ", " ", " ", " ", false};

                                      
printTextFormat pg_utf8format;

typedef struct unicodeStyleRowFormat
{
  const char *horizontal;
  const char *vertical_and_right[2];
  const char *vertical_and_left[2];
} unicodeStyleRowFormat;

typedef struct unicodeStyleColumnFormat
{
  const char *vertical;
  const char *vertical_and_horizontal[2];
  const char *up_and_horizontal[2];
  const char *down_and_horizontal[2];
} unicodeStyleColumnFormat;

typedef struct unicodeStyleBorderFormat
{
  const char *up_and_right;
  const char *vertical;
  const char *down_and_right;
  const char *horizontal;
  const char *down_and_left;
  const char *left_and_right;
} unicodeStyleBorderFormat;

typedef struct unicodeStyleFormat
{
  unicodeStyleRowFormat row_style[2];
  unicodeStyleColumnFormat column_style[2];
  unicodeStyleBorderFormat border_style[2];
  const char *header_nl_left;
  const char *header_nl_right;
  const char *nl_left;
  const char *nl_right;
  const char *wrap_left;
  const char *wrap_right;
  bool wrap_right_border;
} unicodeStyleFormat;

static const unicodeStyleFormat unicode_style = {{
                                                     {
                                                                
                                                         "\342\224\200",
                                                                 
                                                         {"\342\224\234", "\342\225\237"},
                                                                 
                                                         {"\342\224\244", "\342\225\242"},
                                                     },
                                                     {
                                                                
                                                         "\342\225\220",
                                                                 
                                                         {"\342\225\236", "\342\225\240"},
                                                                 
                                                         {"\342\225\241", "\342\225\243"},
                                                     },
                                                 },
    {
        {
                   
            "\342\224\202",
                    
            {"\342\224\274", "\342\225\252"},
                    
            {"\342\224\264", "\342\225\247"},
                    
            {"\342\224\254", "\342\225\244"},
        },
        {
                   
            "\342\225\221",
                    
            {"\342\225\253", "\342\225\254"},
                    
            {"\342\225\250", "\342\225\251"},
                    
            {"\342\225\245", "\342\225\246"},
        },
    },
    {
                    
        {"\342\224\224", "\342\224\202", "\342\224\214", "\342\224\200", "\342\224\220", "\342\224\230"},
                    
        {"\342\225\232", "\342\225\221", "\342\225\224", "\342\225\220", "\342\225\227", "\342\225\235"},
    },
    " ", "\342\206\265",        
    " ", "\342\206\265",        
    "\342\200\246",             
    "\342\200\246",             
    true};

                     
static int
strlen_max_width(unsigned char *str, int *target_width, int encoding);
static void
IsPagerNeeded(const printTableContent *cont, int extra_lines, bool expanded, FILE **fout, bool *is_pager);

static void
print_aligned_vertical(const printTableContent *cont, FILE *fout, bool is_pager);

                                                       
static int
integer_digits(const char *my_str)
{
                             
  if (my_str[0] == '-' || my_str[0] == '+')
  {
    my_str++;
  }
                                         
  return strspn(my_str, "0123456789");
}

                                                                        
static int
additional_numeric_locale_len(const char *my_str)
{
  int int_len = integer_digits(my_str), len = 0;

                                                 
  if (int_len > groupdigits)
  {
    len += ((int_len - 1) / groupdigits) * strlen(thousands_sep);
  }

                                                               
  if (strchr(my_str, '.') != NULL)
  {
    len += strlen(decimal_point) - 1;
  }

  return len;
}

   
                                                                
   
                                                                        
                     
   
                                                     
   
static char *
format_numeric_locale(const char *my_str)
{
  char *new_str;
  int new_len, int_len, leading_digits, i, new_str_pos;

     
                                                                          
                                                                            
     
  if (strspn(my_str, "0123456789+-.eE") != strlen(my_str))
  {
    return pg_strdup(my_str);
  }

  new_len = strlen(my_str) + additional_numeric_locale_len(my_str);
  new_str = pg_malloc(new_len + 1);
  new_str_pos = 0;
  int_len = integer_digits(my_str);

                                                 
  leading_digits = int_len % groupdigits;
  if (leading_digits == 0)
  {
    leading_digits = groupdigits;
  }

                    
  if (my_str[0] == '-' || my_str[0] == '+')
  {
    new_str[new_str_pos++] = my_str[0];
    my_str++;
  }

                                      
  for (i = 0; i < int_len; i++)
  {
                                   
    if (i > 0 && --leading_digits == 0)
    {
      strcpy(&new_str[new_str_pos], thousands_sep);
      new_str_pos += strlen(thousands_sep);
      leading_digits = groupdigits;
    }
    new_str[new_str_pos++] = my_str[i];
  }

                                   
  if (my_str[i] == '.')
  {
    strcpy(&new_str[new_str_pos], decimal_point);
    new_str_pos += strlen(decimal_point);
    i++;
  }

                                                                            
  strcpy(&new_str[new_str_pos], &my_str[i]);

                                                                      
  Assert(strlen(new_str) <= new_len);

  return new_str;
}

   
                                               
   
                                                                 
                                                                
   
static void
fputnbytes(FILE *f, const char *str, size_t n)
{
  while (n-- > 0)
  {
    fputc(*str++, f);
  }
}

static void
print_separator(struct separator sep, FILE *fout)
{
  if (sep.separator_zero)
  {
    fputc('\000', fout);
  }
  else if (sep.separator)
  {
    fputs(sep.separator, fout);
  }
}

   
                                                                            
                                                                          
                                                                              
                                                                               
                                                                        
   
                                                                              
   
static printTableFooter *
footers_with_default(const printTableContent *cont)
{
  if (cont->footers == NULL && cont->opt->default_footer)
  {
    unsigned long total_records;

    total_records = cont->opt->prior_records + cont->nrows;
    snprintf(default_footer, sizeof(default_footer), ngettext("(%lu row)", "(%lu rows)", total_records), total_records);

    return &default_footer_cell;
  }
  else
  {
    return cont->footers;
  }
}

                           
                      
                           

static void
print_unaligned_text(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned int i;
  const char *const *ptr;
  bool need_recordsep = false;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fputs(cont->title, fout);
      print_separator(cont->opt->recordSep, fout);
    }

                       
    if (!opt_tuples_only)
    {
      for (ptr = cont->headers; *ptr; ptr++)
      {
        if (ptr != cont->headers)
        {
          print_separator(cont->opt->fieldSep, fout);
        }
        fputs(*ptr, fout);
      }
      need_recordsep = true;
    }
  }
  else
  {
                                    
    need_recordsep = true;
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (need_recordsep)
    {
      print_separator(cont->opt->recordSep, fout);
      need_recordsep = false;
      if (cancel_pressed)
      {
        break;
      }
    }
    fputs(*ptr, fout);

    if ((i + 1) % cont->ncolumns)
    {
      print_separator(cont->opt->fieldSep, fout);
    }
    else
    {
      need_recordsep = true;
    }
  }

                     
  if (cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

    if (!opt_tuples_only && footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      for (f = footers; f; f = f->next)
      {
        if (need_recordsep)
        {
          print_separator(cont->opt->recordSep, fout);
          need_recordsep = false;
        }
        fputs(f->data, fout);
        need_recordsep = true;
      }
    }

       
                                                                          
                                                                           
                                                          
       
    if (need_recordsep)
    {
      if (cont->opt->recordSep.separator_zero)
      {
        print_separator(cont->opt->recordSep, fout);
      }
      else
      {
        fputc('\n', fout);
      }
    }
  }
}

static void
print_unaligned_vertical(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned int i;
  const char *const *ptr;
  bool need_recordsep = false;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fputs(cont->title, fout);
      need_recordsep = true;
    }
  }
  else
  {
                                    
    need_recordsep = true;
  }

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (need_recordsep)
    {
                                                                       
      print_separator(cont->opt->recordSep, fout);
      print_separator(cont->opt->recordSep, fout);
      need_recordsep = false;
      if (cancel_pressed)
      {
        break;
      }
    }

    fputs(cont->headers[i % cont->ncolumns], fout);
    print_separator(cont->opt->fieldSep, fout);
    fputs(*ptr, fout);

    if ((i + 1) % cont->ncolumns)
    {
      print_separator(cont->opt->recordSep, fout);
    }
    else
    {
      need_recordsep = true;
    }
  }

  if (cont->opt->stop_table)
  {
                       
    if (!opt_tuples_only && cont->footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      print_separator(cont->opt->recordSep, fout);
      for (f = cont->footers; f; f = f->next)
      {
        print_separator(cont->opt->recordSep, fout);
        fputs(f->data, fout);
      }
    }

                                             
    if (need_recordsep)
    {
      if (cont->opt->recordSep.separator_zero)
      {
        print_separator(cont->opt->recordSep, fout);
      }
      else
      {
        fputc('\n', fout);
      }
    }
  }
}

                      
                   
                      

                 
static void
_print_horizontal_line(const unsigned int ncolumns, const unsigned int *widths, unsigned short border, printTextRule pos, const printTextFormat *format, FILE *fout)
{
  const printTextLineFormat *lformat = &format->lrule[pos];
  unsigned int i, j;

  if (border == 1)
  {
    fputs(lformat->hrule, fout);
  }
  else if (border == 2)
  {
    fprintf(fout, "%s%s", lformat->leftvrule, lformat->hrule);
  }

  for (i = 0; i < ncolumns; i++)
  {
    for (j = 0; j < widths[i]; j++)
    {
      fputs(lformat->hrule, fout);
    }

    if (i < ncolumns - 1)
    {
      if (border == 0)
      {
        fputc(' ', fout);
      }
      else
      {
        fprintf(fout, "%s%s%s", lformat->hrule, lformat->midvrule, lformat->hrule);
      }
    }
  }

  if (border == 2)
  {
    fprintf(fout, "%s%s", lformat->hrule, lformat->rightvrule);
  }
  else if (border == 1)
  {
    fputs(lformat->hrule, fout);
  }

  fputc('\n', fout);
}

   
                                    
   
static void
print_aligned_text(const printTableContent *cont, FILE *fout, bool is_pager)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  int encoding = cont->opt->encoding;
  unsigned short opt_border = cont->opt->border;
  const printTextFormat *format = get_line_style(cont->opt);
  const printTextLineFormat *dformat = &format->lrule[PRINT_RULE_DATA];

  unsigned int col_count = 0, cell_count = 0;

  unsigned int i, j;

  unsigned int *width_header, *max_width, *width_wrap, *width_average;
  unsigned int *max_nl_lines,                              
      *curr_nl_line, *max_bytes;
  unsigned char **format_buf;
  unsigned int width_total;
  unsigned int total_header_width;
  unsigned int extra_row_output_lines = 0;
  unsigned int extra_output_lines = 0;

  const char *const *ptr;

  struct lineptr **col_lineptrs;                                          

  bool *header_done;                                               
  int *bytes_output;                                          
  printTextLineWrap *wrap;                                  
  int output_columns = 0;                                    
  bool is_local_pager = false;

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 2)
  {
    opt_border = 2;
  }

  if (cont->ncolumns > 0)
  {
    col_count = cont->ncolumns;
    width_header = pg_malloc0(col_count * sizeof(*width_header));
    width_average = pg_malloc0(col_count * sizeof(*width_average));
    max_width = pg_malloc0(col_count * sizeof(*max_width));
    width_wrap = pg_malloc0(col_count * sizeof(*width_wrap));
    max_nl_lines = pg_malloc0(col_count * sizeof(*max_nl_lines));
    curr_nl_line = pg_malloc0(col_count * sizeof(*curr_nl_line));
    col_lineptrs = pg_malloc0(col_count * sizeof(*col_lineptrs));
    max_bytes = pg_malloc0(col_count * sizeof(*max_bytes));
    format_buf = pg_malloc0(col_count * sizeof(*format_buf));
    header_done = pg_malloc0(col_count * sizeof(*header_done));
    bytes_output = pg_malloc0(col_count * sizeof(*bytes_output));
    wrap = pg_malloc0(col_count * sizeof(*wrap));
  }
  else
  {
    width_header = NULL;
    width_average = NULL;
    max_width = NULL;
    width_wrap = NULL;
    max_nl_lines = NULL;
    curr_nl_line = NULL;
    col_lineptrs = NULL;
    max_bytes = NULL;
    format_buf = NULL;
    header_done = NULL;
    bytes_output = NULL;
    wrap = NULL;
  }

                                                                        
  for (i = 0; i < col_count; i++)
  {
    int width, nl_lines, bytes_required;

    pg_wcssize((const unsigned char *)cont->headers[i], strlen(cont->headers[i]), encoding, &width, &nl_lines, &bytes_required);
    if (width > max_width[i])
    {
      max_width[i] = width;
    }
    if (nl_lines > max_nl_lines[i])
    {
      max_nl_lines[i] = nl_lines;
    }
    if (bytes_required > max_bytes[i])
    {
      max_bytes[i] = bytes_required;
    }
    if (nl_lines > extra_row_output_lines)
    {
      extra_row_output_lines = nl_lines;
    }

    width_header[i] = width;
  }
                                           
  extra_output_lines += extra_row_output_lines;
  extra_row_output_lines = 0;

                                                              
  for (i = 0, ptr = cont->cells; *ptr; ptr++, i++, cell_count++)
  {
    int width, nl_lines, bytes_required;

    pg_wcssize((const unsigned char *)*ptr, strlen(*ptr), encoding, &width, &nl_lines, &bytes_required);

    if (width > max_width[i % col_count])
    {
      max_width[i % col_count] = width;
    }
    if (nl_lines > max_nl_lines[i % col_count])
    {
      max_nl_lines[i % col_count] = nl_lines;
    }
    if (bytes_required > max_bytes[i % col_count])
    {
      max_bytes[i % col_count] = bytes_required;
    }

    width_average[i % col_count] += width;
  }

                                        
  if (col_count != 0 && cell_count != 0)
  {
    int rows = cell_count / col_count;

    for (i = 0; i < col_count; i++)
    {
      width_average[i] /= rows;
    }
  }

                                                            
  if (opt_border == 0)
  {
    width_total = col_count;
  }
  else if (opt_border == 1)
  {
    width_total = col_count * 3 - ((col_count > 0) ? 1 : 0);
  }
  else
  {
    width_total = col_count * 3 + 1;
  }
  total_header_width = width_total;

  for (i = 0; i < col_count; i++)
  {
    width_total += max_width[i];
    total_header_width += width_header[i];
  }

     
                                                                       
                                                                     
                                                                            
                                                                            
                        
     
  for (i = 0; i < col_count; i++)
  {
                                                     
    col_lineptrs[i] = pg_malloc0((max_nl_lines[i] + 1) * sizeof(**col_lineptrs));

    format_buf[i] = pg_malloc(max_bytes[i] + 1);

    col_lineptrs[i]->ptr = format_buf[i];
  }

                                                              
  for (i = 0; i < col_count; i++)
  {
    width_wrap[i] = max_width[i];
  }

     
                                                                      
     
  if (cont->opt->columns > 0)
  {
    output_columns = cont->opt->columns;
  }
  else if ((fout == stdout && isatty(fileno(stdout))) || is_pager)
  {
    if (cont->opt->env_columns > 0)
    {
      output_columns = cont->opt->env_columns;
    }
#ifdef TIOCGWINSZ
    else
    {
      struct winsize screen_size;

      if (ioctl(fileno(stdout), TIOCGWINSZ, &screen_size) != -1)
      {
        output_columns = screen_size.ws_col;
      }
    }
#endif
  }

  if (cont->opt->format == PRINT_WRAPPED)
  {
       
                                                                        
                                                                        
                                                                     
                                                                          
               
       
    if (output_columns > 0 && output_columns >= total_header_width)
    {
                                                
      while (width_total > output_columns)
      {
        double max_ratio = 0;
        int worst_col = -1;

           
                                                                       
                                                                      
                                                                
                                                    
           
        for (i = 0; i < col_count; i++)
        {
          if (width_average[i] && width_wrap[i] > width_header[i])
          {
                                                            
            double ratio;

            ratio = (double)width_wrap[i] / width_average[i] + max_width[i] * 0.01;
            if (ratio > max_ratio)
            {
              max_ratio = ratio;
              worst_col = i;
            }
          }
        }

                                                     
        if (worst_col == -1)
        {
          break;
        }

                                                     
        width_wrap[worst_col]--;
        width_total--;
      }
    }
  }

     
                                                                             
                                                                         
                                                                          
                                           
     
  if (cont->opt->expanded == 2 && output_columns > 0 && cont->ncolumns > 1 && (output_columns < total_header_width || output_columns < width_total))
  {
    print_aligned_vertical(cont, fout, is_pager);
    goto cleanup;
  }

                                                             
  if (!is_pager && fout == stdout && output_columns > 0 && (output_columns < total_header_width || output_columns < width_total))
  {
    fout = PageOutput(INT_MAX, cont->opt);                  
    is_pager = is_local_pager = true;
  }

                                                            
  if (!is_pager && fout == stdout)
  {
                                                                
    for (i = 0, ptr = cont->cells; *ptr; ptr++, cell_count++)
    {
      int width, nl_lines, bytes_required;

      pg_wcssize((const unsigned char *)*ptr, strlen(*ptr), encoding, &width, &nl_lines, &bytes_required);

         
                                                                    
                                                                        
         
      if (width > 0 && width_wrap[i])
      {
        unsigned int extra_lines;

                                                                       
        extra_lines = ((width - 1) / width_wrap[i]) + nl_lines - 1;
        if (extra_lines > extra_row_output_lines)
        {
          extra_row_output_lines = extra_lines;
        }
      }

                                                               
      if (++i >= col_count)
      {
        i = 0;
                                                                   
        extra_output_lines += extra_row_output_lines;
        extra_row_output_lines = 0;
      }
    }
    IsPagerNeeded(cont, extra_output_lines, false, &fout, &is_pager);
    is_local_pager = is_pager;
  }

                      
  if (cont->opt->start_table)
  {
                     
    if (cont->title && !opt_tuples_only)
    {
      int width, height;

      pg_wcssize((const unsigned char *)cont->title, strlen(cont->title), encoding, &width, &height, NULL);
      if (width >= width_total)
      {
                     
        fprintf(fout, "%s\n", cont->title);
      }
      else
      {
                      
        fprintf(fout, "%-*s%s\n", (width_total - width) / 2, "", cont->title);
      }
    }

                       
    if (!opt_tuples_only)
    {
      int more_col_wrapping;
      int curr_nl_line;

      if (opt_border == 2)
      {
        _print_horizontal_line(col_count, width_wrap, opt_border, PRINT_RULE_TOP, format, fout);
      }

      for (i = 0; i < col_count; i++)
      {
        pg_wcsformat((const unsigned char *)cont->headers[i], strlen(cont->headers[i]), encoding, col_lineptrs[i], max_nl_lines[i]);
      }

      more_col_wrapping = col_count;
      curr_nl_line = 0;
      if (col_count > 0)
      {
        memset(header_done, false, col_count * sizeof(bool));
      }
      while (more_col_wrapping)
      {
        if (opt_border == 2)
        {
          fputs(dformat->leftvrule, fout);
        }

        for (i = 0; i < cont->ncolumns; i++)
        {
          struct lineptr *this_line = col_lineptrs[i] + curr_nl_line;
          unsigned int nbspace;

          if (opt_border != 0 || (!format->wrap_right_border && i > 0))
          {
            fputs(curr_nl_line ? format->header_nl_left : " ", fout);
          }

          if (!header_done[i])
          {
            nbspace = width_wrap[i] - this_line->width;

                          
            fprintf(fout, "%-*s%s%-*s", nbspace / 2, "", this_line->ptr, (nbspace + 1) / 2, "");

            if (!(this_line + 1)->ptr)
            {
              more_col_wrapping--;
              header_done[i] = 1;
            }
          }
          else
          {
            fprintf(fout, "%*s", width_wrap[i], "");
          }

          if (opt_border != 0 || format->wrap_right_border)
          {
            fputs(!header_done[i] ? format->header_nl_right : " ", fout);
          }

          if (opt_border != 0 && col_count > 0 && i < col_count - 1)
          {
            fputs(dformat->midvrule, fout);
          }
        }
        curr_nl_line++;

        if (opt_border == 2)
        {
          fputs(dformat->rightvrule, fout);
        }
        fputc('\n', fout);
      }

      _print_horizontal_line(col_count, width_wrap, opt_border, PRINT_RULE_MIDDLE, format, fout);
    }
  }

                                     
  for (i = 0, ptr = cont->cells; *ptr; i += col_count, ptr += col_count)
  {
    bool more_lines;

    if (cancel_pressed)
    {
      break;
    }

       
                         
       
    for (j = 0; j < col_count; j++)
    {
      pg_wcsformat((const unsigned char *)ptr[j], strlen(ptr[j]), encoding, col_lineptrs[j], max_nl_lines[j]);
      curr_nl_line[j] = 0;
    }

    memset(bytes_output, 0, col_count * sizeof(int));

       
                                                                       
                                                                      
                                                      
       
    do
    {
      more_lines = false;

                       
      if (opt_border == 2)
      {
        fputs(dformat->leftvrule, fout);
      }

                           
      for (j = 0; j < col_count; j++)
      {
                                                        
        struct lineptr *this_line = &col_lineptrs[j][curr_nl_line[j]];
        int bytes_to_output;
        int chars_to_output = width_wrap[j];
        bool finalspaces = (opt_border == 2 || (col_count > 0 && j < col_count - 1));

                                                  
        if (opt_border != 0)
        {
          if (wrap[j] == PRINT_LINE_WRAP_WRAP)
          {
            fputs(format->wrap_left, fout);
          }
          else if (wrap[j] == PRINT_LINE_WRAP_NEWLINE)
          {
            fputs(format->nl_left, fout);
          }
          else
          {
            fputc(' ', fout);
          }
        }

        if (!this_line->ptr)
        {
                                                                
          if (finalspaces)
          {
            fprintf(fout, "%*s", chars_to_output, "");
          }
        }
        else
        {
                                                               
          bytes_to_output = strlen_max_width(this_line->ptr + bytes_output[j], &chars_to_output, encoding);

             
                                                                   
                                                                    
                                                                   
                                                               
             
          if (chars_to_output > width_wrap[j])
          {
            chars_to_output = width_wrap[j];
          }

          if (cont->aligns[j] == 'r')                         
          {
                              
            fprintf(fout, "%*s", width_wrap[j] - chars_to_output, "");
            fputnbytes(fout, (char *)(this_line->ptr + bytes_output[j]), bytes_to_output);
          }
          else                        
          {
                               
            fputnbytes(fout, (char *)(this_line->ptr + bytes_output[j]), bytes_to_output);
          }

          bytes_output[j] += bytes_to_output;

                                             
          if (*(this_line->ptr + bytes_output[j]) != '\0')
          {
            more_lines = true;
          }
          else
          {
                                              
            curr_nl_line[j]++;
            if (col_lineptrs[j][curr_nl_line[j]].ptr != NULL)
            {
              more_lines = true;
            }
            bytes_output[j] = 0;
          }
        }

                                                               
        wrap[j] = PRINT_LINE_WRAP_NONE;
        if (col_lineptrs[j][curr_nl_line[j]].ptr != NULL)
        {
          if (bytes_output[j] != 0)
          {
            wrap[j] = PRINT_LINE_WRAP_WRAP;
          }
          else if (curr_nl_line[j] != 0)
          {
            wrap[j] = PRINT_LINE_WRAP_NEWLINE;
          }
        }

           
                                                                   
                                                     
           
        if (cont->aligns[j] != 'r')                        
        {
          if (finalspaces || wrap[j] == PRINT_LINE_WRAP_WRAP || wrap[j] == PRINT_LINE_WRAP_NEWLINE)
          {
            fprintf(fout, "%*s", width_wrap[j] - chars_to_output, "");
          }
        }

                                                   
        if (wrap[j] == PRINT_LINE_WRAP_WRAP)
        {
          fputs(format->wrap_right, fout);
        }
        else if (wrap[j] == PRINT_LINE_WRAP_NEWLINE)
        {
          fputs(format->nl_right, fout);
        }
        else if (opt_border == 2 || (col_count > 0 && j < col_count - 1))
        {
          fputc(' ', fout);
        }

                                                          
        if (opt_border != 0 && (col_count > 0 && j < col_count - 1))
        {
          if (wrap[j + 1] == PRINT_LINE_WRAP_WRAP)
          {
            fputs(format->midvrule_wrap, fout);
          }
          else if (wrap[j + 1] == PRINT_LINE_WRAP_NEWLINE)
          {
            fputs(format->midvrule_nl, fout);
          }
          else if (col_lineptrs[j + 1][curr_nl_line[j + 1]].ptr == NULL)
          {
            fputs(format->midvrule_blank, fout);
          }
          else
          {
            fputs(dformat->midvrule, fout);
          }
        }
      }

                             
      if (opt_border == 2)
      {
        fputs(dformat->rightvrule, fout);
      }
      fputc('\n', fout);

    } while (more_lines);
  }

  if (cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

    if (opt_border == 2 && !cancel_pressed)
    {
      _print_horizontal_line(col_count, width_wrap, opt_border, PRINT_RULE_BOTTOM, format, fout);
    }

                       
    if (footers && !opt_tuples_only && !cancel_pressed)
    {
      printTableFooter *f;

      for (f = footers; f; f = f->next)
      {
        fprintf(fout, "%s\n", f->data);
      }
    }

    fputc('\n', fout);
  }

cleanup:
                
  for (i = 0; i < col_count; i++)
  {
    free(col_lineptrs[i]);
    free(format_buf[i]);
  }
  free(width_header);
  free(width_average);
  free(max_width);
  free(width_wrap);
  free(max_nl_lines);
  free(curr_nl_line);
  free(col_lineptrs);
  free(max_bytes);
  free(format_buf);
  free(header_done);
  free(bytes_output);
  free(wrap);

  if (is_local_pager)
  {
    ClosePager(fout);
  }
}

static void
print_aligned_vertical_line(const printTextFormat *format, const unsigned short opt_border, unsigned long record, unsigned int hwidth, unsigned int dwidth, printTextRule pos, FILE *fout)
{
  const printTextLineFormat *lformat = &format->lrule[pos];
  unsigned int i;
  int reclen = 0;

  if (opt_border == 2)
  {
    fprintf(fout, "%s%s", lformat->leftvrule, lformat->hrule);
  }
  else if (opt_border == 1)
  {
    fputs(lformat->hrule, fout);
  }

  if (record)
  {
    if (opt_border == 0)
    {
      reclen = fprintf(fout, "* Record %lu", record);
    }
    else
    {
      reclen = fprintf(fout, "[ RECORD %lu ]", record);
    }
  }
  if (opt_border != 2)
  {
    reclen++;
  }
  if (reclen < 0)
  {
    reclen = 0;
  }
  for (i = reclen; i < hwidth; i++)
  {
    fputs(opt_border > 0 ? lformat->hrule : " ", fout);
  }
  reclen -= hwidth;

  if (opt_border > 0)
  {
    if (reclen-- <= 0)
    {
      fputs(lformat->hrule, fout);
    }
    if (reclen-- <= 0)
    {
      fputs(lformat->midvrule, fout);
    }
    if (reclen-- <= 0)
    {
      fputs(lformat->hrule, fout);
    }
  }
  else
  {
    if (reclen-- <= 0)
    {
      fputc(' ', fout);
    }
  }
  if (reclen < 0)
  {
    reclen = 0;
  }
  for (i = reclen; i < dwidth; i++)
  {
    fputs(opt_border > 0 ? lformat->hrule : " ", fout);
  }
  if (opt_border == 2)
  {
    fprintf(fout, "%s%s", lformat->hrule, lformat->rightvrule);
  }
  fputc('\n', fout);
}

static void
print_aligned_vertical(const printTableContent *cont, FILE *fout, bool is_pager)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  const printTextFormat *format = get_line_style(cont->opt);
  const printTextLineFormat *dformat = &format->lrule[PRINT_RULE_DATA];
  int encoding = cont->opt->encoding;
  unsigned long record = cont->opt->prior_records + 1;
  const char *const *ptr;
  unsigned int i, hwidth = 0, dwidth = 0, hheight = 1, dheight = 1, hformatsize = 0, dformatsize = 0;
  struct lineptr *hlineptr, *dlineptr;
  bool is_local_pager = false, hmultiline = false, dmultiline = false;
  int output_columns = 0;                                   

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 2)
  {
    opt_border = 2;
  }

  if (cont->cells[0] == NULL && cont->opt->start_table && cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

    if (!opt_tuples_only && !cancel_pressed && footers)
    {
      printTableFooter *f;

      for (f = footers; f; f = f->next)
      {
        fprintf(fout, "%s\n", f->data);
      }
    }

    fputc('\n', fout);

    return;
  }

     
                                                                           
                                                                             
                                                                    
     
  if (!is_pager)
  {
    IsPagerNeeded(cont, 0, true, &fout, &is_pager);
    is_local_pager = is_pager;
  }

                                                   
  for (i = 0; i < cont->ncolumns; i++)
  {
    int width, height, fs;

    pg_wcssize((const unsigned char *)cont->headers[i], strlen(cont->headers[i]), encoding, &width, &height, &fs);
    if (width > hwidth)
    {
      hwidth = width;
    }
    if (height > hheight)
    {
      hheight = height;
      hmultiline = true;
    }
    if (fs > hformatsize)
    {
      hformatsize = fs;
    }
  }

                              
  for (i = 0, ptr = cont->cells; *ptr; ptr++, i++)
  {
    int width, height, fs;

    pg_wcssize((const unsigned char *)*ptr, strlen(*ptr), encoding, &width, &height, &fs);
    if (width > dwidth)
    {
      dwidth = width;
    }
    if (height > dheight)
    {
      dheight = height;
      dmultiline = true;
    }
    if (fs > dformatsize)
    {
      dformatsize = fs;
    }
  }

     
                                                                     
                
     
  dlineptr = pg_malloc((sizeof(*dlineptr)) * (dheight + 1));
  hlineptr = pg_malloc((sizeof(*hlineptr)) * (hheight + 1));

  dlineptr->ptr = pg_malloc(dformatsize);
  hlineptr->ptr = pg_malloc(hformatsize);

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fprintf(fout, "%s\n", cont->title);
    }
  }

     
                                                                      
     
  if (cont->opt->columns > 0)
  {
    output_columns = cont->opt->columns;
  }
  else if ((fout == stdout && isatty(fileno(stdout))) || is_pager)
  {
    if (cont->opt->env_columns > 0)
    {
      output_columns = cont->opt->env_columns;
    }
#ifdef TIOCGWINSZ
    else
    {
      struct winsize screen_size;

      if (ioctl(fileno(stdout), TIOCGWINSZ, &screen_size) != -1)
      {
        output_columns = screen_size.ws_col;
      }
    }
#endif
  }

     
                                                        
     
  if (cont->opt->format == PRINT_WRAPPED)
  {
    unsigned int swidth, rwidth = 0, newdwidth;

    if (opt_border == 0)
    {
         
                                                                      
                                                                    
                                                                      
                                                                       
         
      swidth = 1;

                                                                  
      if (hmultiline)
      {
        swidth++;
      }
    }
    else if (opt_border == 1)
    {
         
                                                                    
                                                                  
         
      swidth = 3;

                                                                       
      if (hmultiline && (format == &pg_asciiformat_old))
      {
        swidth++;
      }
    }
    else
    {
         
                                                                      
                                                                       
                                                                       
                                 
         
      swidth = 7;
    }

                                                                      
    if (dmultiline && opt_border < 2 && format != &pg_asciiformat_old)
    {
      swidth++;
    }

                                                          
    if (!opt_tuples_only)
    {
      if (cont->nrows > 0)
      {
        rwidth = 1 + (int)log10(cont->nrows);
      }
      if (opt_border == 0)
      {
        rwidth += 9;                  
      }
      else if (opt_border == 1)
      {
        rwidth += 12;                     
      }
      else
      {
        rwidth += 15;                        
      }
    }

                                                               
    for (;;)
    {
      unsigned int width;

                                                 
      width = hwidth + swidth + dwidth;
                                                
      if (width < rwidth)
      {
        width = rwidth;
      }

      if (output_columns > 0)
      {
        unsigned int min_width;

                                                                       
        min_width = hwidth + swidth + 3;
                                                                     
        if (min_width < rwidth)
        {
          min_width = rwidth;
        }

        if (output_columns >= width)
        {
                                                     
                                                                 
          newdwidth = width - hwidth - swidth;
        }
        else if (output_columns < min_width)
        {
                                                 
          newdwidth = min_width - hwidth - swidth;
        }
        else
        {
                                                      
          newdwidth = output_columns - hwidth - swidth;
        }
      }
      else
      {
                                                                 
                                                               
        newdwidth = width - hwidth - swidth;
      }

         
                                                                         
                                                          
         
      if (newdwidth < dwidth && !dmultiline && opt_border < 2 && format != &pg_asciiformat_old)
      {
        dmultiline = true;
        swidth++;
      }
      else
      {
        break;
      }
    }

    dwidth = newdwidth;
  }

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    printTextRule pos;
    int dline, hline, dcomplete, hcomplete, offset, chars_to_output;

    if (cancel_pressed)
    {
      break;
    }

    if (i == 0)
    {
      pos = PRINT_RULE_TOP;
    }
    else
    {
      pos = PRINT_RULE_MIDDLE;
    }

                                                                     
    if (i % cont->ncolumns == 0)
    {
      unsigned int lhwidth = hwidth;

      if ((opt_border < 2) && (hmultiline) && (format == &pg_asciiformat_old))
      {
        lhwidth++;                             
      }

      if (!opt_tuples_only)
      {
        print_aligned_vertical_line(format, opt_border, record++, lhwidth, dwidth, pos, fout);
      }
      else if (i != 0 || !cont->opt->start_table || opt_border == 2)
      {
        print_aligned_vertical_line(format, opt_border, 0, lhwidth, dwidth, pos, fout);
      }
    }

                           
    pg_wcsformat((const unsigned char *)cont->headers[i % cont->ncolumns], strlen(cont->headers[i % cont->ncolumns]), encoding, hlineptr, hheight);
                         
    pg_wcsformat((const unsigned char *)*ptr, strlen(*ptr), encoding, dlineptr, dheight);

       
                                                                          
                                                  
       
    dline = hline = 0;
    dcomplete = hcomplete = 0;
    offset = 0;
    chars_to_output = dlineptr[dline].width;
    while (!dcomplete || !hcomplete)
    {
                       
      if (opt_border == 2)
      {
        fprintf(fout, "%s", dformat->leftvrule);
      }

                                                                     
      if (!hcomplete)
      {
        int swidth = hwidth, target_width = hwidth;

           
                                             
           
        if ((opt_border == 2) || (hmultiline && (format == &pg_asciiformat_old)))
        {
          fputs(hline ? format->header_nl_left : " ", fout);
        }

           
                       
           
        strlen_max_width(hlineptr[hline].ptr, &target_width, encoding);
        fprintf(fout, "%-s", hlineptr[hline].ptr);

           
                  
           
        swidth -= target_width;
        if (swidth > 0)
        {
          fprintf(fout, "%*s", swidth, " ");
        }

           
                                                   
           
        if (hlineptr[hline + 1].ptr)
        {
                                                          
          if ((opt_border > 0) || (hmultiline && (format != &pg_asciiformat_old)))
          {
            fputs(format->header_nl_right, fout);
          }
          hline++;
        }
        else
        {
                                                    
          if ((opt_border > 0) || (hmultiline && (format != &pg_asciiformat_old)))
          {
            fputs(" ", fout);
          }
          hcomplete = 1;
        }
      }
      else
      {
        unsigned int swidth = hwidth + opt_border;

        if ((opt_border < 2) && (hmultiline) && (format == &pg_asciiformat_old))
        {
          swidth++;
        }

        if ((opt_border == 0) && (format != &pg_asciiformat_old) && (hmultiline))
        {
          swidth++;
        }

        fprintf(fout, "%*s", swidth, " ");
      }

                     
      if (opt_border > 0)
      {
        if (offset)
        {
          fputs(format->midvrule_wrap, fout);
        }
        else if (dline == 0)
        {
          fputs(dformat->midvrule, fout);
        }
        else
        {
          fputs(format->midvrule_nl, fout);
        }
      }

                
      if (!dcomplete)
      {
        int target_width = dwidth, bytes_to_output, swidth = dwidth;

           
                                         
           
        fputs(offset == 0 ? " " : format->wrap_left, fout);

           
                     
           
        bytes_to_output = strlen_max_width(dlineptr[dline].ptr + offset, &target_width, encoding);
        fputnbytes(fout, (char *)(dlineptr[dline].ptr + offset), bytes_to_output);

        chars_to_output -= target_width;
        offset += bytes_to_output;

                    
        swidth -= target_width;

        if (chars_to_output)
        {
                                           
          if ((opt_border > 1) || (dmultiline && (format != &pg_asciiformat_old)))
          {
            if (swidth > 0)
            {
              fprintf(fout, "%*s", swidth, " ");
            }
            fputs(format->wrap_right, fout);
          }
        }
        else if (dlineptr[dline + 1].ptr)
        {
                                               
          if ((opt_border > 1) || (dmultiline && (format != &pg_asciiformat_old)))
          {
            if (swidth > 0)
            {
              fprintf(fout, "%*s", swidth, " ");
            }
            fputs(format->nl_right, fout);
          }
          dline++;
          offset = 0;
          chars_to_output = dlineptr[dline].width;
        }
        else
        {
                                           
          if (opt_border > 1)
          {
            if (swidth > 0)
            {
              fprintf(fout, "%*s", swidth, " ");
            }
            fputs(" ", fout);
          }
          dcomplete = 1;
        }

                          
        if (opt_border == 2)
        {
          fputs(dformat->rightvrule, fout);
        }

        fputs("\n", fout);
      }
      else
      {
           
                                                                       
                                               
           
        if (opt_border < 2)
        {
          fputs("\n", fout);
        }
        else
        {
          fprintf(fout, "%*s  %s\n", dwidth, "", dformat->rightvrule);
        }
      }
    }
  }

  if (cont->opt->stop_table)
  {
    if (opt_border == 2 && !cancel_pressed)
    {
      print_aligned_vertical_line(format, opt_border, 0, hwidth, dwidth, PRINT_RULE_BOTTOM, fout);
    }

                       
    if (!opt_tuples_only && cont->footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      if (opt_border < 2)
      {
        fputc('\n', fout);
      }
      for (f = cont->footers; f; f = f->next)
      {
        fprintf(fout, "%s\n", f->data);
      }
    }

    fputc('\n', fout);
  }

  free(hlineptr->ptr);
  free(dlineptr->ptr);
  free(hlineptr);
  free(dlineptr);

  if (is_local_pager)
  {
    ClosePager(fout);
  }
}

                        
                   
                        

static void
csv_escaped_print(const char *str, FILE *fout)
{
  const char *p;

  fputc('"', fout);
  for (p = str; *p; p++)
  {
    if (*p == '"')
    {
      fputc('"', fout);                                
    }
    fputc(*p, fout);
  }
  fputc('"', fout);
}

static void
csv_print_field(const char *str, FILE *fout, char sep)
{
                     
                                                                            
                                                     
                                      
                                          
                                  
                                                 
                                                                        
                                                                       
                                                                       
                                                     
                     
     
  if (strchr(str, sep) != NULL || strcspn(str, "\r\n\"") != strlen(str) || strcmp(str, "\\.") == 0 || sep == '\\' || sep == '.')
  {
    csv_escaped_print(str, fout);
  }
  else
  {
    fputs(str, fout);
  }
}

static void
print_csv_text(const printTableContent *cont, FILE *fout)
{
  const char *const *ptr;
  int i;

  if (cancel_pressed)
  {
    return;
  }

     
                                                                         
                                          
     
                                                                         
                                                                            
                                                      
     
  if (cont->opt->start_table && !cont->opt->tuples_only)
  {
                       
    for (ptr = cont->headers; *ptr; ptr++)
    {
      if (ptr != cont->headers)
      {
        fputc(cont->opt->csvFieldSep[0], fout);
      }
      csv_print_field(*ptr, fout, cont->opt->csvFieldSep[0]);
    }
    fputc('\n', fout);
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    csv_print_field(*ptr, fout, cont->opt->csvFieldSep[0]);
    if ((i + 1) % cont->ncolumns)
    {
      fputc(cont->opt->csvFieldSep[0], fout);
    }
    else
    {
      fputc('\n', fout);
    }
  }
}

static void
print_csv_vertical(const printTableContent *cont, FILE *fout)
{
  const char *const *ptr;
  int i;

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (cancel_pressed)
    {
      return;
    }

                              
    csv_print_field(cont->headers[i % cont->ncolumns], fout, cont->opt->csvFieldSep[0]);

                               
    fputc(cont->opt->csvFieldSep[0], fout);

                           
    csv_print_field(*ptr, fout, cont->opt->csvFieldSep[0]);

    fputc('\n', fout);
  }
}

                        
               
                        

void
html_escaped_print(const char *in, FILE *fout)
{
  const char *p;
  bool leading_space = true;

  for (p = in; *p; p++)
  {
    switch (*p)
    {
    case '&':
      fputs("&amp;", fout);
      break;
    case '<':
      fputs("&lt;", fout);
      break;
    case '>':
      fputs("&gt;", fout);
      break;
    case '\n':
      fputs("<br />\n", fout);
      break;
    case '"':
      fputs("&quot;", fout);
      break;
    case ' ':
                                                     
      if (leading_space)
      {
        fputs("&nbsp;", fout);
      }
      else
      {
        fputs(" ", fout);
      }
      break;
    default:
      fputc(*p, fout);
    }
    if (*p != ' ')
    {
      leading_space = false;
    }
  }
}

static void
print_html_text(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  const char *opt_table_attr = cont->opt->tableAttr;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->start_table)
  {
    fprintf(fout, "<table border=\"%d\"", opt_border);
    if (opt_table_attr)
    {
      fprintf(fout, " %s", opt_table_attr);
    }
    fputs(">\n", fout);

                     
    if (!opt_tuples_only && cont->title)
    {
      fputs("  <caption>", fout);
      html_escaped_print(cont->title, fout);
      fputs("</caption>\n", fout);
    }

                       
    if (!opt_tuples_only)
    {
      fputs("  <tr>\n", fout);
      for (ptr = cont->headers; *ptr; ptr++)
      {
        fputs("    <th align=\"center\">", fout);
        html_escaped_print(*ptr, fout);
        fputs("</th>\n", fout);
      }
      fputs("  </tr>\n", fout);
    }
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (i % cont->ncolumns == 0)
    {
      if (cancel_pressed)
      {
        break;
      }
      fputs("  <tr valign=\"top\">\n", fout);
    }

    fprintf(fout, "    <td align=\"%s\">", cont->aligns[(i) % cont->ncolumns] == 'r' ? "right" : "left");
                                    
    if ((*ptr)[strspn(*ptr, " \t")] == '\0')
    {
      fputs("&nbsp; ", fout);
    }
    else
    {
      html_escaped_print(*ptr, fout);
    }

    fputs("</td>\n", fout);

    if ((i + 1) % cont->ncolumns == 0)
    {
      fputs("  </tr>\n", fout);
    }
  }

  if (cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

    fputs("</table>\n", fout);

                       
    if (!opt_tuples_only && footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      fputs("<p>", fout);
      for (f = footers; f; f = f->next)
      {
        html_escaped_print(f->data, fout);
        fputs("<br />\n", fout);
      }
      fputs("</p>", fout);
    }

    fputc('\n', fout);
  }
}

static void
print_html_vertical(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  const char *opt_table_attr = cont->opt->tableAttr;
  unsigned long record = cont->opt->prior_records + 1;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->start_table)
  {
    fprintf(fout, "<table border=\"%d\"", opt_border);
    if (opt_table_attr)
    {
      fprintf(fout, " %s", opt_table_attr);
    }
    fputs(">\n", fout);

                     
    if (!opt_tuples_only && cont->title)
    {
      fputs("  <caption>", fout);
      html_escaped_print(cont->title, fout);
      fputs("</caption>\n", fout);
    }
  }

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (i % cont->ncolumns == 0)
    {
      if (cancel_pressed)
      {
        break;
      }
      if (!opt_tuples_only)
      {
        fprintf(fout, "\n  <tr><td colspan=\"2\" align=\"center\">Record %lu</td></tr>\n", record++);
      }
      else
      {
        fputs("\n  <tr><td colspan=\"2\">&nbsp;</td></tr>\n", fout);
      }
    }
    fputs("  <tr valign=\"top\">\n"
          "    <th>",
        fout);
    html_escaped_print(cont->headers[i % cont->ncolumns], fout);
    fputs("</th>\n", fout);

    fprintf(fout, "    <td align=\"%s\">", cont->aligns[i % cont->ncolumns] == 'r' ? "right" : "left");
                                    
    if ((*ptr)[strspn(*ptr, " \t")] == '\0')
    {
      fputs("&nbsp; ", fout);
    }
    else
    {
      html_escaped_print(*ptr, fout);
    }

    fputs("</td>\n  </tr>\n", fout);
  }

  if (cont->opt->stop_table)
  {
    fputs("</table>\n", fout);

                       
    if (!opt_tuples_only && cont->footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      fputs("<p>", fout);
      for (f = cont->footers; f; f = f->next)
      {
        html_escaped_print(f->data, fout);
        fputs("<br />\n", fout);
      }
      fputs("</p>", fout);
    }

    fputc('\n', fout);
  }
}

                           
                  
                           

static void
asciidoc_escaped_print(const char *in, FILE *fout)
{
  const char *p;

  for (p = in; *p; p++)
  {
    switch (*p)
    {
    case '|':
      fputs("\\|", fout);
      break;
    default:
      fputc(*p, fout);
    }
  }
}

static void
print_asciidoc_text(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->start_table)
  {
                                                                     
    fputs("\n", fout);

                     
    if (!opt_tuples_only && cont->title)
    {
      fputs(".", fout);
      fputs(cont->title, fout);
      fputs("\n", fout);
    }

                                          
    fprintf(fout, "[%scols=\"", !opt_tuples_only ? "options=\"header\"," : "");
    for (i = 0; i < cont->ncolumns; i++)
    {
      if (i != 0)
      {
        fputs(",", fout);
      }
      fprintf(fout, "%s", cont->aligns[(i) % cont->ncolumns] == 'r' ? ">l" : "<l");
    }
    fputs("\"", fout);
    switch (opt_border)
    {
    case 0:
      fputs(",frame=\"none\",grid=\"none\"", fout);
      break;
    case 1:
      fputs(",frame=\"none\"", fout);
      break;
    case 2:
      fputs(",frame=\"all\",grid=\"all\"", fout);
      break;
    }
    fputs("]\n", fout);
    fputs("|====\n", fout);

                       
    if (!opt_tuples_only)
    {
      for (ptr = cont->headers; *ptr; ptr++)
      {
        if (ptr != cont->headers)
        {
          fputs(" ", fout);
        }
        fputs("^l|", fout);
        asciidoc_escaped_print(*ptr, fout);
      }
      fputs("\n", fout);
    }
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (i % cont->ncolumns == 0)
    {
      if (cancel_pressed)
      {
        break;
      }
    }

    if (i % cont->ncolumns != 0)
    {
      fputs(" ", fout);
    }
    fputs("|", fout);

                                         
    if ((*ptr)[strspn(*ptr, " \t")] == '\0')
    {
      if ((i + 1) % cont->ncolumns != 0)
      {
        fputs(" ", fout);
      }
    }
    else
    {
      asciidoc_escaped_print(*ptr, fout);
    }

    if ((i + 1) % cont->ncolumns == 0)
    {
      fputs("\n", fout);
    }
  }

  fputs("|====\n", fout);

  if (cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

                       
    if (!opt_tuples_only && footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      fputs("\n....\n", fout);
      for (f = footers; f; f = f->next)
      {
        fputs(f->data, fout);
        fputs("\n", fout);
      }
      fputs("....\n", fout);
    }
  }
}

static void
print_asciidoc_vertical(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned long record = cont->opt->prior_records + 1;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->start_table)
  {
                                                                     
    fputs("\n", fout);

                     
    if (!opt_tuples_only && cont->title)
    {
      fputs(".", fout);
      fputs(cont->title, fout);
      fputs("\n", fout);
    }

                                          
    fputs("[cols=\"h,l\"", fout);
    switch (opt_border)
    {
    case 0:
      fputs(",frame=\"none\",grid=\"none\"", fout);
      break;
    case 1:
      fputs(",frame=\"none\"", fout);
      break;
    case 2:
      fputs(",frame=\"all\",grid=\"all\"", fout);
      break;
    }
    fputs("]\n", fout);
    fputs("|====\n", fout);
  }

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    if (i % cont->ncolumns == 0)
    {
      if (cancel_pressed)
      {
        break;
      }
      if (!opt_tuples_only)
      {
        fprintf(fout, "2+^|Record %lu\n", record++);
      }
      else
      {
        fputs("2+|\n", fout);
      }
    }

    fputs("<l|", fout);
    asciidoc_escaped_print(cont->headers[i % cont->ncolumns], fout);

    fprintf(fout, " %s|", cont->aligns[i % cont->ncolumns] == 'r' ? ">l" : "<l");
                                    
    if ((*ptr)[strspn(*ptr, " \t")] == '\0')
    {
      fputs(" ", fout);
    }
    else
    {
      asciidoc_escaped_print(*ptr, fout);
    }
    fputs("\n", fout);
  }

  fputs("|====\n", fout);

  if (cont->opt->stop_table)
  {
                       
    if (!opt_tuples_only && cont->footers != NULL && !cancel_pressed)
    {
      printTableFooter *f;

      fputs("\n....\n", fout);
      for (f = cont->footers; f; f = f->next)
      {
        fputs(f->data, fout);
        fputs("\n", fout);
      }
      fputs("....\n", fout);
    }
  }
}

                           
               
                           

static void
latex_escaped_print(const char *in, FILE *fout)
{
  const char *p;

  for (p = in; *p; p++)
  {
    switch (*p)
    {
         
                                                                
                                                              
                                                                  
         
    case '#':
      fputs("\\#", fout);
      break;
    case '$':
      fputs("\\$", fout);
      break;
    case '%':
      fputs("\\%", fout);
      break;
    case '&':
      fputs("\\&", fout);
      break;
    case '<':
      fputs("\\textless{}", fout);
      break;
    case '>':
      fputs("\\textgreater{}", fout);
      break;
    case '\\':
      fputs("\\textbackslash{}", fout);
      break;
    case '^':
      fputs("\\^{}", fout);
      break;
    case '_':
      fputs("\\_", fout);
      break;
    case '{':
      fputs("\\{", fout);
      break;
    case '|':
      fputs("\\textbar{}", fout);
      break;
    case '}':
      fputs("\\}", fout);
      break;
    case '~':
      fputs("\\~{}", fout);
      break;
    case '\n':
                                                                
      fputs("\\\\", fout);
      break;
    default:
      fputc(*p, fout);
    }
  }
}

static void
print_latex_text(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 3)
  {
    opt_border = 3;
  }

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fputs("\\begin{center}\n", fout);
      latex_escaped_print(cont->title, fout);
      fputs("\n\\end{center}\n\n", fout);
    }

                                                          
    fputs("\\begin{tabular}{", fout);

    if (opt_border >= 2)
    {
      fputs("| ", fout);
    }
    for (i = 0; i < cont->ncolumns; i++)
    {
      fputc(*(cont->aligns + i), fout);
      if (opt_border != 0 && i < cont->ncolumns - 1)
      {
        fputs(" | ", fout);
      }
    }
    if (opt_border >= 2)
    {
      fputs(" |", fout);
    }

    fputs("}\n", fout);

    if (!opt_tuples_only && opt_border >= 2)
    {
      fputs("\\hline\n", fout);
    }

                       
    if (!opt_tuples_only)
    {
      for (i = 0, ptr = cont->headers; i < cont->ncolumns; i++, ptr++)
      {
        if (i != 0)
        {
          fputs(" & ", fout);
        }
        fputs("\\textit{", fout);
        latex_escaped_print(*ptr, fout);
        fputc('}', fout);
      }
      fputs(" \\\\\n", fout);
      fputs("\\hline\n", fout);
    }
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    latex_escaped_print(*ptr, fout);

    if ((i + 1) % cont->ncolumns == 0)
    {
      fputs(" \\\\\n", fout);
      if (opt_border == 3)
      {
        fputs("\\hline\n", fout);
      }
      if (cancel_pressed)
      {
        break;
      }
    }
    else
    {
      fputs(" & ", fout);
    }
  }

  if (cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

    if (opt_border == 2)
    {
      fputs("\\hline\n", fout);
    }

    fputs("\\end{tabular}\n\n\\noindent ", fout);

                       
    if (footers && !opt_tuples_only && !cancel_pressed)
    {
      printTableFooter *f;

      for (f = footers; f; f = f->next)
      {
        latex_escaped_print(f->data, fout);
        fputs(" \\\\\n", fout);
      }
    }

    fputc('\n', fout);
  }
}

                           
                       
                           

static void
print_latex_longtable_text(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned int i;
  const char *opt_table_attr = cont->opt->tableAttr;
  const char *next_opt_table_attr_char = opt_table_attr;
  const char *last_opt_table_attr_char = NULL;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 3)
  {
    opt_border = 3;
  }

  if (cont->opt->start_table)
  {
                                                          
    fputs("\\begin{longtable}{", fout);

    if (opt_border >= 2)
    {
      fputs("| ", fout);
    }

    for (i = 0; i < cont->ncolumns; i++)
    {
                                                                       
                                                                         
      if (*(cont->aligns + i) == 'l' && opt_table_attr)
      {
#define LONGTABLE_WHITESPACE " \t\n"

                                     
        next_opt_table_attr_char += strspn(next_opt_table_attr_char, LONGTABLE_WHITESPACE);
                              
        if (next_opt_table_attr_char[0] != '\0')
        {
          fputs("p{", fout);
          fwrite(next_opt_table_attr_char, strcspn(next_opt_table_attr_char, LONGTABLE_WHITESPACE), 1, fout);
          last_opt_table_attr_char = next_opt_table_attr_char;
          next_opt_table_attr_char += strcspn(next_opt_table_attr_char, LONGTABLE_WHITESPACE);
          fputs("\\textwidth}", fout);
        }
                                
        else if (last_opt_table_attr_char != NULL)
        {
          fputs("p{", fout);
          fwrite(last_opt_table_attr_char, strcspn(last_opt_table_attr_char, LONGTABLE_WHITESPACE), 1, fout);
          fputs("\\textwidth}", fout);
        }
        else
        {
          fputc('l', fout);
        }
      }
      else
      {
        fputc(*(cont->aligns + i), fout);
      }

      if (opt_border != 0 && i < cont->ncolumns - 1)
      {
        fputs(" | ", fout);
      }
    }

    if (opt_border >= 2)
    {
      fputs(" |", fout);
    }

    fputs("}\n", fout);

                       
    if (!opt_tuples_only)
    {
                     
      if (opt_border >= 2)
      {
        fputs("\\toprule\n", fout);
      }
      for (i = 0, ptr = cont->headers; i < cont->ncolumns; i++, ptr++)
      {
        if (i != 0)
        {
          fputs(" & ", fout);
        }
        fputs("\\small\\textbf{\\textit{", fout);
        latex_escaped_print(*ptr, fout);
        fputs("}}", fout);
      }
      fputs(" \\\\\n", fout);
      fputs("\\midrule\n\\endfirsthead\n", fout);

                           
      if (opt_border >= 2)
      {
        fputs("\\toprule\n", fout);
      }
      for (i = 0, ptr = cont->headers; i < cont->ncolumns; i++, ptr++)
      {
        if (i != 0)
        {
          fputs(" & ", fout);
        }
        fputs("\\small\\textbf{\\textit{", fout);
        latex_escaped_print(*ptr, fout);
        fputs("}}", fout);
      }
      fputs(" \\\\\n", fout);
                                                                        
      if (opt_border != 3)
      {
        fputs("\\midrule\n", fout);
      }
      fputs("\\endhead\n", fout);

                                
      if (!opt_tuples_only && cont->title)
      {
                                                                   
        if (opt_border == 2)
        {
          fputs("\\bottomrule\n", fout);
        }
        fputs("\\caption[", fout);
        latex_escaped_print(cont->title, fout);
        fputs(" (Continued)]{", fout);
        latex_escaped_print(cont->title, fout);
        fputs("}\n\\endfoot\n", fout);
        if (opt_border == 2)
        {
          fputs("\\bottomrule\n", fout);
        }
        fputs("\\caption[", fout);
        latex_escaped_print(cont->title, fout);
        fputs("]{", fout);
        latex_escaped_print(cont->title, fout);
        fputs("}\n\\endlastfoot\n", fout);
      }
                                     
      else if (opt_border >= 2)
      {
        fputs("\\bottomrule\n\\endfoot\n", fout);
        fputs("\\bottomrule\n\\endlastfoot\n", fout);
      }
    }
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
                                    
    if (i != 0 && i % cont->ncolumns != 0)
    {
      fputs("\n&\n", fout);
    }
    fputs("\\raggedright{", fout);
    latex_escaped_print(*ptr, fout);
    fputc('}', fout);
    if ((i + 1) % cont->ncolumns == 0)
    {
      fputs(" \\tabularnewline\n", fout);
      if (opt_border == 3)
      {
        fputs(" \\hline\n", fout);
      }
    }
    if (cancel_pressed)
    {
      break;
    }
  }

  if (cont->opt->stop_table)
  {
    fputs("\\end{longtable}\n", fout);
  }
}

static void
print_latex_vertical(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned long record = cont->opt->prior_records + 1;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 2)
  {
    opt_border = 2;
  }

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fputs("\\begin{center}\n", fout);
      latex_escaped_print(cont->title, fout);
      fputs("\n\\end{center}\n\n", fout);
    }

                                                          
    fputs("\\begin{tabular}{", fout);
    if (opt_border == 0)
    {
      fputs("cl", fout);
    }
    else if (opt_border == 1)
    {
      fputs("c|l", fout);
    }
    else if (opt_border == 2)
    {
      fputs("|c|l|", fout);
    }
    fputs("}\n", fout);
  }

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
                    
    if (i % cont->ncolumns == 0)
    {
      if (cancel_pressed)
      {
        break;
      }
      if (!opt_tuples_only)
      {
        if (opt_border == 2)
        {
          fputs("\\hline\n", fout);
          fprintf(fout, "\\multicolumn{2}{|c|}{\\textit{Record %lu}} \\\\\n", record++);
        }
        else
        {
          fprintf(fout, "\\multicolumn{2}{c}{\\textit{Record %lu}} \\\\\n", record++);
        }
      }
      if (opt_border >= 1)
      {
        fputs("\\hline\n", fout);
      }
    }

    latex_escaped_print(cont->headers[i % cont->ncolumns], fout);
    fputs(" & ", fout);
    latex_escaped_print(*ptr, fout);
    fputs(" \\\\\n", fout);
  }

  if (cont->opt->stop_table)
  {
    if (opt_border == 2)
    {
      fputs("\\hline\n", fout);
    }

    fputs("\\end{tabular}\n\n\\noindent ", fout);

                       
    if (cont->footers && !opt_tuples_only && !cancel_pressed)
    {
      printTableFooter *f;

      for (f = cont->footers; f; f = f->next)
      {
        latex_escaped_print(f->data, fout);
        fputs(" \\\\\n", fout);
      }
    }

    fputc('\n', fout);
  }
}

                           
                  
                           

static void
troff_ms_escaped_print(const char *in, FILE *fout)
{
  const char *p;

  for (p = in; *p; p++)
  {
    switch (*p)
    {
    case '\\':
      fputs("\\(rs", fout);
      break;
    default:
      fputc(*p, fout);
    }
  }
}

static void
print_troff_ms_text(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned int i;
  const char *const *ptr;

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 2)
  {
    opt_border = 2;
  }

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fputs(".LP\n.DS C\n", fout);
      troff_ms_escaped_print(cont->title, fout);
      fputs("\n.DE\n", fout);
    }

                                                          
    fputs(".LP\n.TS\n", fout);
    if (opt_border == 2)
    {
      fputs("center box;\n", fout);
    }
    else
    {
      fputs("center;\n", fout);
    }

    for (i = 0; i < cont->ncolumns; i++)
    {
      fputc(*(cont->aligns + i), fout);
      if (opt_border > 0 && i < cont->ncolumns - 1)
      {
        fputs(" | ", fout);
      }
    }
    fputs(".\n", fout);

                       
    if (!opt_tuples_only)
    {
      for (i = 0, ptr = cont->headers; i < cont->ncolumns; i++, ptr++)
      {
        if (i != 0)
        {
          fputc('\t', fout);
        }
        fputs("\\fI", fout);
        troff_ms_escaped_print(*ptr, fout);
        fputs("\\fP", fout);
      }
      fputs("\n_\n", fout);
    }
  }

                   
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
    troff_ms_escaped_print(*ptr, fout);

    if ((i + 1) % cont->ncolumns == 0)
    {
      fputc('\n', fout);
      if (cancel_pressed)
      {
        break;
      }
    }
    else
    {
      fputc('\t', fout);
    }
  }

  if (cont->opt->stop_table)
  {
    printTableFooter *footers = footers_with_default(cont);

    fputs(".TE\n.DS L\n", fout);

                       
    if (footers && !opt_tuples_only && !cancel_pressed)
    {
      printTableFooter *f;

      for (f = footers; f; f = f->next)
      {
        troff_ms_escaped_print(f->data, fout);
        fputc('\n', fout);
      }
    }

    fputs(".DE\n", fout);
  }
}

static void
print_troff_ms_vertical(const printTableContent *cont, FILE *fout)
{
  bool opt_tuples_only = cont->opt->tuples_only;
  unsigned short opt_border = cont->opt->border;
  unsigned long record = cont->opt->prior_records + 1;
  unsigned int i;
  const char *const *ptr;
  unsigned short current_format = 0;                               

  if (cancel_pressed)
  {
    return;
  }

  if (opt_border > 2)
  {
    opt_border = 2;
  }

  if (cont->opt->start_table)
  {
                     
    if (!opt_tuples_only && cont->title)
    {
      fputs(".LP\n.DS C\n", fout);
      troff_ms_escaped_print(cont->title, fout);
      fputs("\n.DE\n", fout);
    }

                                                          
    fputs(".LP\n.TS\n", fout);
    if (opt_border == 2)
    {
      fputs("center box;\n", fout);
    }
    else
    {
      fputs("center;\n", fout);
    }

                      
    if (opt_tuples_only)
    {
      fputs("c l;\n", fout);
    }
  }
  else
  {
    current_format = 2;                                    
  }

                     
  for (i = 0, ptr = cont->cells; *ptr; i++, ptr++)
  {
                    
    if (i % cont->ncolumns == 0)
    {
      if (cancel_pressed)
      {
        break;
      }
      if (!opt_tuples_only)
      {
        if (current_format != 1)
        {
          if (opt_border == 2 && record > 1)
          {
            fputs("_\n", fout);
          }
          if (current_format != 0)
          {
            fputs(".T&\n", fout);
          }
          fputs("c s.\n", fout);
          current_format = 1;
        }
        fprintf(fout, "\\fIRecord %lu\\fP\n", record++);
      }
      if (opt_border >= 1)
      {
        fputs("_\n", fout);
      }
    }

    if (!opt_tuples_only)
    {
      if (current_format != 2)
      {
        if (current_format != 0)
        {
          fputs(".T&\n", fout);
        }
        if (opt_border != 1)
        {
          fputs("c l.\n", fout);
        }
        else
        {
          fputs("c | l.\n", fout);
        }
        current_format = 2;
      }
    }

    troff_ms_escaped_print(cont->headers[i % cont->ncolumns], fout);
    fputc('\t', fout);
    troff_ms_escaped_print(*ptr, fout);

    fputc('\n', fout);
  }

  if (cont->opt->stop_table)
  {
    fputs(".TE\n.DS L\n", fout);

                       
    if (cont->footers && !opt_tuples_only && !cancel_pressed)
    {
      printTableFooter *f;

      for (f = cont->footers; f; f = f->next)
      {
        troff_ms_escaped_print(f->data, fout);
        fputc('\n', fout);
      }
    }

    fputs(".DE\n", fout);
  }
}

                                  
                         
                                  

   
                        
   
                                                                          
                                     
   
                                                          
   
void
disable_sigpipe_trap(void)
{
#ifndef WIN32
  pqsignal(SIGPIPE, SIG_IGN);
#endif
}

   
                        
   
                                                                         
                                                                     
   
                                                                              
                                                                       
                                                                           
                                                                            
                                                                              
                                                                             
   
                                                          
   
void
restore_sigpipe_trap(void)
{
#ifndef WIN32
  pqsignal(SIGPIPE, always_ignore_sigpipe ? SIG_IGN : SIG_DFL);
#endif
}

   
                          
   
                                                                   
   
void
set_sigpipe_trap_state(bool ignore)
{
  always_ignore_sigpipe = ignore;
}

   
              
   
                                                                  
   
                                                  
   
FILE *
PageOutput(int lines, const printTableOpt *topt)
{
                                                               
  if (topt && topt->pager && isatty(fileno(stdin)) && isatty(fileno(stdout)))
  {
#ifdef TIOCGWINSZ
    unsigned short int pager = topt->pager;
    int min_lines = topt->pager_min_lines;
    int result;
    struct winsize screen_size;

    result = ioctl(fileno(stdout), TIOCGWINSZ, &screen_size);

                                           
    if (result == -1 || (lines >= screen_size.ws_row && lines >= min_lines) || pager > 1)
#endif
    {
      const char *pagerprog;
      FILE *pagerpipe;

      pagerprog = getenv("PSQL_PAGER");
      if (!pagerprog)
      {
        pagerprog = getenv("PAGER");
      }
      if (!pagerprog)
      {
        pagerprog = DEFAULT_PAGER;
      }
      else
      {
                                                                   
        if (strspn(pagerprog, " \t\r\n") == strlen(pagerprog))
        {
          return stdout;
        }
      }
      disable_sigpipe_trap();
      pagerpipe = popen(pagerprog, "w");
      if (pagerpipe)
      {
        return pagerpipe;
      }
                                                          
      restore_sigpipe_trap();
    }
  }

  return stdout;
}

   
              
   
                                              
   
void
ClosePager(FILE *pagerpipe)
{
  if (pagerpipe && pagerpipe != stdout)
  {
       
                                                          
       
                                                                           
                                                                          
                                                                   
                    
       
    if (cancel_pressed)
    {
      fprintf(pagerpipe, _("Interrupted\n"));
    }

    pclose(pagerpipe);
    restore_sigpipe_trap();
  }
}

   
                                       
                                                               
   
                                                                       
                                                                  
   
                                                                               
          
   
void
printTableInit(printTableContent *const content, const printTableOpt *opt, const char *title, const int ncolumns, const int nrows)
{
  content->opt = opt;
  content->title = title;
  content->ncolumns = ncolumns;
  content->nrows = nrows;

  content->headers = pg_malloc0((ncolumns + 1) * sizeof(*content->headers));

  content->cells = pg_malloc0((ncolumns * nrows + 1) * sizeof(*content->cells));

  content->cellmustfree = NULL;
  content->footers = NULL;

  content->aligns = pg_malloc0((ncolumns + 1) * sizeof(*content->align));

  content->header = content->headers;
  content->cell = content->cells;
  content->footer = content->footers;
  content->align = content->aligns;
  content->cellsadded = 0;
}

   
                              
   
                                                                         
                                                               
   
                                                                            
                                                 
   
                                                                             
           
   
void
printTableAddHeader(printTableContent *const content, char *header, const bool translate, const char align)
{
#ifndef ENABLE_NLS
  (void)translate;                       
#endif

  if (content->header >= content->headers + content->ncolumns)
  {
    fprintf(stderr,
        _("Cannot add header to table content: "
          "column count of %d exceeded.\n"),
        content->ncolumns);
    exit(EXIT_FAILURE);
  }

  *content->header = (char *)mbvalidate((unsigned char *)header, content->opt->encoding);
#ifdef ENABLE_NLS
  if (translate)
  {
    *content->header = _(*content->header);
  }
#endif
  content->header++;

  *content->align = align;
  content->align++;
}

   
                            
   
                                                                               
                                                     
   
                                                                          
                                               
   
                                                                         
                                                                     
   
void
printTableAddCell(printTableContent *const content, char *cell, const bool translate, const bool mustfree)
{
#ifndef ENABLE_NLS
  (void)translate;                       
#endif

  if (content->cellsadded >= content->ncolumns * content->nrows)
  {
    fprintf(stderr,
        _("Cannot add cell to table content: "
          "total cell count of %d exceeded.\n"),
        content->ncolumns * content->nrows);
    exit(EXIT_FAILURE);
  }

  *content->cell = (char *)mbvalidate((unsigned char *)cell, content->opt->encoding);

#ifdef ENABLE_NLS
  if (translate)
  {
    *content->cell = _(*content->cell);
  }
#endif

  if (mustfree)
  {
    if (content->cellmustfree == NULL)
    {
      content->cellmustfree = pg_malloc0((content->ncolumns * content->nrows + 1) * sizeof(bool));
    }

    content->cellmustfree[content->cellsadded] = true;
  }
  content->cell++;
  content->cellsadded++;
}

   
                              
   
                                                                             
                                                                            
   
                                                                         
                                                                                
                                                                              
                                                                       
                          
   
void
printTableAddFooter(printTableContent *const content, const char *footer)
{
  printTableFooter *f;

  f = pg_malloc0(sizeof(*f));
  f->data = pg_strdup(footer);

  if (content->footers == NULL)
  {
    content->footers = f;
  }
  else
  {
    content->footer->next = f;
  }

  content->footer = f;
}

   
                                                
   
                                                                                
                                                                              
   
                                                                            
           
   
void
printTableSetFooter(printTableContent *const content, const char *footer)
{
  if (content->footers != NULL)
  {
    free(content->footer->data);
    content->footer->data = pg_strdup(footer);
  }
  else
  {
    printTableAddFooter(content, footer);
  }
}

   
                                             
   
                                                                           
                           
   
void
printTableCleanup(printTableContent *const content)
{
  if (content->cellmustfree)
  {
    int i;

    for (i = 0; i < content->nrows * content->ncolumns; i++)
    {
      if (content->cellmustfree[i])
      {
        free(unconstify(char *, content->cells[i]));
      }
    }
    free(content->cellmustfree);
    content->cellmustfree = NULL;
  }
  free(content->headers);
  free(content->cells);
  free(content->aligns);

  content->opt = NULL;
  content->title = NULL;
  content->headers = NULL;
  content->cells = NULL;
  content->aligns = NULL;
  content->header = NULL;
  content->cell = NULL;
  content->align = NULL;

  if (content->footers)
  {
    for (content->footer = content->footers; content->footer;)
    {
      printTableFooter *f;

      f = content->footer;
      content->footer = f->next;
      free(f->data);
      free(f);
    }
  }
  content->footers = NULL;
  content->footer = NULL;
}

   
                 
   
                           
   
static void
IsPagerNeeded(const printTableContent *cont, int extra_lines, bool expanded, FILE **fout, bool *is_pager)
{
  if (*fout == stdout)
  {
    int lines;

    if (expanded)
    {
      lines = (cont->ncolumns + 1) * cont->nrows;
    }
    else
    {
      lines = cont->nrows + 1;
    }

    if (!cont->opt->tuples_only)
    {
      printTableFooter *f;

         
                                                                  
                                                   
         
      for (f = cont->footers; f; f = f->next)
      {
        lines++;
      }
    }

    *fout = PageOutput(lines + extra_lines, cont->opt);
    *is_pager = (*fout != stdout);
  }
  else
  {
    *is_pager = false;
  }
}

   
                                                         
   
                                           
                           
                                                                           
                                                                         
   
void
printTable(const printTableContent *cont, FILE *fout, bool is_pager, FILE *flog)
{
  bool is_local_pager = false;

  if (cancel_pressed)
  {
    return;
  }

  if (cont->opt->format == PRINT_NOTHING)
  {
    return;
  }

                                                     
  if (!is_pager && cont->opt->format != PRINT_ALIGNED && cont->opt->format != PRINT_WRAPPED)
  {
    IsPagerNeeded(cont, 0, (cont->opt->expanded == 1), &fout, &is_pager);
    is_local_pager = is_pager;
  }

                       

  if (flog)
  {
    print_aligned_text(cont, flog, false);
  }

  switch (cont->opt->format)
  {
  case PRINT_UNALIGNED:
    if (cont->opt->expanded == 1)
    {
      print_unaligned_vertical(cont, fout);
    }
    else
    {
      print_unaligned_text(cont, fout);
    }
    break;
  case PRINT_ALIGNED:
  case PRINT_WRAPPED:

       
                                                                      
                                                                       
                     
       
    if (cont->opt->expanded == 1 || (cont->opt->expanded == 2 && is_pager))
    {
      print_aligned_vertical(cont, fout, is_pager);
    }
    else
    {
      print_aligned_text(cont, fout, is_pager);
    }
    break;
  case PRINT_CSV:
    if (cont->opt->expanded == 1)
    {
      print_csv_vertical(cont, fout);
    }
    else
    {
      print_csv_text(cont, fout);
    }
    break;
  case PRINT_HTML:
    if (cont->opt->expanded == 1)
    {
      print_html_vertical(cont, fout);
    }
    else
    {
      print_html_text(cont, fout);
    }
    break;
  case PRINT_ASCIIDOC:
    if (cont->opt->expanded == 1)
    {
      print_asciidoc_vertical(cont, fout);
    }
    else
    {
      print_asciidoc_text(cont, fout);
    }
    break;
  case PRINT_LATEX:
    if (cont->opt->expanded == 1)
    {
      print_latex_vertical(cont, fout);
    }
    else
    {
      print_latex_text(cont, fout);
    }
    break;
  case PRINT_LATEX_LONGTABLE:
    if (cont->opt->expanded == 1)
    {
      print_latex_vertical(cont, fout);
    }
    else
    {
      print_latex_longtable_text(cont, fout);
    }
    break;
  case PRINT_TROFF_MS:
    if (cont->opt->expanded == 1)
    {
      print_troff_ms_vertical(cont, fout);
    }
    else
    {
      print_troff_ms_text(cont, fout);
    }
    break;
  default:
    fprintf(stderr, _("invalid output format (internal error): %d"), cont->opt->format);
    exit(EXIT_FAILURE);
  }

  if (is_local_pager)
  {
    ClosePager(fout);
  }
}

   
                                   
   
                                        
                           
                           
                                                                           
                                                                        
   
void
printQuery(const PGresult *result, const printQueryOpt *opt, FILE *fout, bool is_pager, FILE *flog)
{
  printTableContent cont;
  int i, r, c;

  if (cancel_pressed)
  {
    return;
  }

  printTableInit(&cont, &opt->topt, opt->title, PQnfields(result), PQntuples(result));

                                                                 
  Assert(opt->translate_columns == NULL || opt->n_translate_columns >= cont.ncolumns);

  for (i = 0; i < cont.ncolumns; i++)
  {
    printTableAddHeader(&cont, PQfname(result, i), opt->translate_header, column_type_alignment(PQftype(result, i)));
  }

                 
  for (r = 0; r < cont.nrows; r++)
  {
    for (c = 0; c < cont.ncolumns; c++)
    {
      char *cell;
      bool mustfree = false;
      bool translate;

      if (PQgetisnull(result, r, c))
      {
        cell = opt->nullPrint ? opt->nullPrint : "";
      }
      else
      {
        cell = PQgetvalue(result, r, c);
        if (cont.aligns[c] == 'r' && opt->topt.numericLocale)
        {
          cell = format_numeric_locale(cell);
          mustfree = true;
        }
      }

      translate = (opt->translate_columns && opt->translate_columns[c]);
      printTableAddCell(&cont, cell, translate, mustfree);
    }
  }

                   
  if (opt->footers)
  {
    char **footer;

    for (footer = opt->footers; *footer; footer++)
    {
      printTableAddFooter(&cont, *footer);
    }
  }

  printTable(&cont, fout, is_pager, flog);
  printTableCleanup(&cont);
}

char
column_type_alignment(Oid ftype)
{
  char align;

  switch (ftype)
  {
  case INT2OID:
  case INT4OID:
  case INT8OID:
  case FLOAT4OID:
  case FLOAT8OID:
  case NUMERICOID:
  case OIDOID:
  case XIDOID:
  case CIDOID:
  case CASHOID:
    align = 'r';
    break;
  default:
    align = 'l';
    break;
  }
  return align;
}

void
setDecimalLocale(void)
{
  struct lconv *extlconv;

  extlconv = localeconv();

                                                  
  if (*extlconv->decimal_point)
  {
    decimal_point = pg_strdup(extlconv->decimal_point);
  }
  else
  {
    decimal_point = ".";                          
  }

     
                                                                             
                                                                            
                                                                       
                                                                          
                      
     
  groupdigits = *extlconv->grouping;
  if (groupdigits <= 0 || groupdigits > 6)
  {
    groupdigits = 3;                  
  }

                                                          
                                           
  if (*extlconv->thousands_sep)
  {
    thousands_sep = pg_strdup(extlconv->thousands_sep);
  }
                                                                         
  else if (strcmp(decimal_point, ",") != 0)
  {
    thousands_sep = ",";
  }
  else
  {
    thousands_sep = ".";
  }
}

                                        
const printTextFormat *
get_line_style(const printTableOpt *opt)
{
     
                                                                         
                                                                      
               
     
  if (opt->line_style != NULL)
  {
    return opt->line_style;
  }
  else
  {
    return &pg_asciiformat;
  }
}

void
refresh_utf8format(const printTableOpt *opt)
{
  printTextFormat *popt = &pg_utf8format;

  const unicodeStyleBorderFormat *border;
  const unicodeStyleRowFormat *header;
  const unicodeStyleColumnFormat *column;

  popt->name = "unicode";

  border = &unicode_style.border_style[opt->unicode_border_linestyle];
  header = &unicode_style.row_style[opt->unicode_header_linestyle];
  column = &unicode_style.column_style[opt->unicode_column_linestyle];

  popt->lrule[PRINT_RULE_TOP].hrule = border->horizontal;
  popt->lrule[PRINT_RULE_TOP].leftvrule = border->down_and_right;
  popt->lrule[PRINT_RULE_TOP].midvrule = column->down_and_horizontal[opt->unicode_border_linestyle];
  popt->lrule[PRINT_RULE_TOP].rightvrule = border->down_and_left;

  popt->lrule[PRINT_RULE_MIDDLE].hrule = header->horizontal;
  popt->lrule[PRINT_RULE_MIDDLE].leftvrule = header->vertical_and_right[opt->unicode_border_linestyle];
  popt->lrule[PRINT_RULE_MIDDLE].midvrule = column->vertical_and_horizontal[opt->unicode_header_linestyle];
  popt->lrule[PRINT_RULE_MIDDLE].rightvrule = header->vertical_and_left[opt->unicode_border_linestyle];

  popt->lrule[PRINT_RULE_BOTTOM].hrule = border->horizontal;
  popt->lrule[PRINT_RULE_BOTTOM].leftvrule = border->up_and_right;
  popt->lrule[PRINT_RULE_BOTTOM].midvrule = column->up_and_horizontal[opt->unicode_border_linestyle];
  popt->lrule[PRINT_RULE_BOTTOM].rightvrule = border->left_and_right;

           
  popt->lrule[PRINT_RULE_DATA].hrule = "";
  popt->lrule[PRINT_RULE_DATA].leftvrule = border->vertical;
  popt->lrule[PRINT_RULE_DATA].midvrule = column->vertical;
  popt->lrule[PRINT_RULE_DATA].rightvrule = border->vertical;

  popt->midvrule_nl = column->vertical;
  popt->midvrule_wrap = column->vertical;
  popt->midvrule_blank = column->vertical;

                                  
  popt->header_nl_left = unicode_style.header_nl_left;
  popt->header_nl_right = unicode_style.header_nl_right;
  popt->nl_left = unicode_style.nl_left;
  popt->nl_right = unicode_style.nl_right;
  popt->wrap_left = unicode_style.wrap_left;
  popt->wrap_right = unicode_style.wrap_right;
  popt->wrap_right_border = unicode_style.wrap_right_border;

  return;
}

   
                                                                       
                                                                             
                                                                    
   
static int
strlen_max_width(unsigned char *str, int *target_width, int encoding)
{
  unsigned char *start = str;
  unsigned char *end = str + strlen((char *)str);
  int curr_width = 0;

  while (str < end)
  {
    int char_width = PQdsplen((char *)str, encoding);

       
                                                                      
                                                                         
                                                                       
                  
       
    if (*target_width < curr_width + char_width && curr_width != 0)
    {
      break;
    }

    curr_width += char_width;

    str += PQmblen((char *)str, encoding);

    if (str > end)                                   
    {
      str = end;
    }
  }

  *target_width = curr_width;

  return str - start;
}
