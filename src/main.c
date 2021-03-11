#include <dmcp.h>

#include "pforth.h"
#include "main.h"
#include "menu.h"

void program_main()
{
  run_menu_item_app = run_menu_item;
  menu_line_str_app = menu_line_str;

  pfDoForth(NULL, "dm42", 1);
}
