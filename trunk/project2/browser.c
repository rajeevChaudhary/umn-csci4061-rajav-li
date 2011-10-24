#include "wrapper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

extern int errno;
comm_channel channel[UNRECLAIMED_TAB_COUNTER];

/*
 * Name:		uri_entered_cb
 * Input arguments:'entry'-address bar where the url was entered
 *			 'data'-auxiliary data sent along with the event
 * Output arguments:void
 * Function:	When the user hits the enter after entering the url
 *			in the address bar, 'activate' event is generated
 *			for the Widget Entry, for which 'uri_entered_cb'
 *			callback is called. Controller-tab captures this event
 *			and sends the browsing request to the router(/parent)
 *			process.
 */

void uri_entered_cb(GtkWidget* entry, gpointer data)
{
	if(!data)
		return;
	browser_window* b_window = (browser_window*)data;
        comm_channel channel = b_window->channel;
	// Get the tab index where the URL is to be rendered
	int tab_index = query_tab_id_for_request(entry, data);
	if(tab_index <= 0)
	{
		//Append code for error handling
	}

	// Get the URL.
	char* uri = get_entered_uri(entry);

	// Prepare 'request' packet to send to router (/parent) process.
	// Append your code here
}





/*

 * Name:		new_tab_created_cb
 * Input arguments:	'button' - whose click generated this callback
 *			'data' - auxillary data passed along for handling
 *			this event.
 * Output arguments:    void
 * Function:		This is the callback function for the 'create_new_tab'
 *			event which is generated when the user clicks the '+'
 *			button in the controller-tab. The controller-tab
 *			redirects the request to the parent (/router) process
 *			which then creates a new child process for creating
 *			and managing this new tab.
 */
void new_tab_created_cb(GtkButton *button, gpointer data)

{
	if(!data)
		return;
 	int tab_index = ((browser_window*)data)->tab_index;
	comm_channel channel = ((browser_window*)data)->channel;

	// Create a new request of type CREATE_TAB

        child_req_to_parent new_req;
	//Append your code here

}

// Returns 1 on success or 0 on failure.
void fork_controller(int pipe_fildes[2])

{
    if (pipe(pipe_fildes) == -1)
    {
        perror("fork_controller: Failed to create the pipe");
        return 0;
    }
    else
    {
        //fork
    }
}


int main()

{
	/** <ROUTER> **/

	// index 0 is CONTROLLER pipe
	// indices 1-10 are URL-RENDERING pipes
	int child_pipes[11][2];

    fork_controller(child_pipes[0]);

	return 0;
}

