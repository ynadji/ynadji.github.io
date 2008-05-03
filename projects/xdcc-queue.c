/**
 * XDCC-Queue is a plugin for X-Chat written in the C programming language
 * It was written by Yacin Nadji and is licensed under the GNU GPL license.
 *
 * If you have any questions or comments, feel free to contact me by email
 * ynadji@iit.edu
 *
 * Thank you, I hope you find this script as useful as I do! :D
 */

#include <xchat/xchat-plugin.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PNAME "XDCC-Queue"
#define PDESC "Queues files for an XDCC Bot based on pack # range"
#define PVERSION "0.1"

static xchat_plugin *ph; /* plugin handle */
static xchat_hook *myhook; /* hook, checks to see if file is done before queueing another */
static char *botname;
int curr_file;
int fpack;
int lpack;
int range;

int xchat_plugin_init(xchat_plugin *plugin_handle,
                      char **plugin_name,
                      char **plugin_desc,
                      char **plugin_version,
                      char *arg);

static void xdcc_queue(char *word[], char *word_eol[], void *userdata);
static int is_file_done(void);
static int send_req(void);
static int stop(char *word[], char *word_eol[], void *userdata);

/**
 * Grabs range values and the botname, and stores them for further use
 */
static void xdcc_queue(char *word[], char *word_eol[], void *userdata)
{
  fpack = atoi(word[3]);
  lpack = atoi(word[4]);
  range = lpack - fpack;
  curr_file = fpack;
  if ( (botname = strdup(word[2])) == NULL)
    printf("Memory couldn't be allocated =(\n");
  xchat_printf(ph, "Receiving pack range %d - %d from %s", fpack, lpack, word[2]);
}


/**
 * Checks to see if the bot is still sending a file
 * if the bot is, the function returns 0, and the hook command repeats.
 * If the bot isn't sending anymore, it returns 1, and the next file will be
 * sent.
 */
static int is_file_done(void)
{
  xchat_list *list;
  int val;
  char *curr_name;
  list = xchat_list_get(ph, "dcc");
  if (list)
  {
    printf("List exists\n");
    while (xchat_list_next(ph, list))
    {
      if ( (curr_name = strdup(xchat_list_str(ph, list, "nick"))) == NULL)
        printf("Memory couldn't be allocated =(\n");
      printf("Botname from global: --%s--\n", botname);
      printf("Botname from DCC list: --%s--\n", curr_name);
      if (strcasecmp(botname, curr_name) == 0)
      {
        // DCC Status: 0-Queued 1-Active 2-Failed 3-Done 4-Connecting 5-Aborted
        val = xchat_list_int(ph, list, "status");
        printf("Current status of the file: %d\n", val);
        if (val == 0 || val == 1 || val == 4)
          return 0;
      }
    }
  }
  return 1;
}


/**
 * Sends the next file
 */
static int send_req(void)
{
  if (myhook == NULL)
   myhook = xchat_hook_timer(ph, 10000, send_req, NULL);
  printf("Range is: %d\n", range);
  if (range > 0 && curr_file <= lpack)
  {
    if (is_file_done())
    {
      xchat_commandf(ph, "ctcp %s xdcc send #%d", botname, curr_file);
      curr_file++;
    }
  }
  else
  {
    xchat_print(ph, "XDCC-Queue: Range specified is invalid\n");
  }
  return 1;
}

/**
 * Stops send_req from processing, use it to turn off
 * the functionality of xdcc-queue
 */
static int stop(char *word[], char *word_eol[], void *userdata)
{
  if (myhook != NULL)
	{
		xchat_unhook(ph, myhook);
		myhook = NULL;
		xchat_print(ph, "File-check stopped. Type /start to resume\n");
	}

	return XCHAT_EAT_ALL;
}

int xchat_plugin_init(xchat_plugin *plugin_handle,
                      char **plugin_name,
                      char **plugin_desc,
                      char **plugin_version,
                      char *arg)
{
   /* we need to save this for use with any xchat_* functions */
   ph = plugin_handle;

   /* tell xchat our info */
   *plugin_name = PNAME;
   *plugin_desc = PDESC;
   *plugin_version = PVERSION;

   myhook = xchat_hook_timer(ph, 10000, send_req, NULL);
   xchat_hook_command(ph, "start", XCHAT_PRI_NORM, send_req, "Usage: after /xdcc-queue, do /start to send files", 0);
   xchat_hook_command(ph, "xdcc-queue", XCHAT_PRI_NORM, xdcc_queue, "Usage: /xdcc-queue BOT fpack# lpack#", 0);
   xchat_hook_command(ph, "stop", XCHAT_PRI_NORM, stop, NULL, NULL);
   xchat_print(ph, "******************************************\nXDCC-Queue: Plugin loaded successfully!\n/help xdcc-queue for usage\n******************************************");

   return 1;       /* return 1 for success */
}
