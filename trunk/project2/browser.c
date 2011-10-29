// Arbitrary; must be > 0
#define PARENT 1
#define CHILD 2

#define READ 0
#define WRITE 1

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
comm_channel channel[UNRECLAIMED_TAB_COUNTER + 1];
bool channel_alive[UNRECLAIMED_TAB_COUNTER + 1];

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


// Forking function -- returns PARENT, CHILD, or 0 for failure
int fork_controller()

{
    if (pipe(channel[0].parent_to_child_fd) == -1 ||
        pipe(channel[0].child_to_parent_fd) == -1)
    {
        perror("fork_controller: Failed to create one or both pipes");
        return 0;
    }
    else
    {
        channel_alive[0] = true;
        
        switch ( fork() )
        {
            case -1:
                perror("fork_controller: Failed to fork");
                return 0;
                
            case 0:     //@ Parent code (ROUTER)
                close(channel[0].parent_to_child_fd[READ]);
                close(channel[0].child_to_parent_fd[WRITE]);
                                
                fcntl(channel[0].child_to_parent_fd[READ], F_SETFL, O_NONBLOCK);
                
                return PARENT;
                
            default:    //@ Child code (CONTROLLER)
                close(channel[0].child_to_parent_fd[READ]);
                close(channel[0].parent_to_child_fd[WRITE]);
                
                browser_window* bwindow;
                
                create_browser(CONTROLLER_TAB, 0, (void*)&new_tab_created_cb, (void*)&uri_entered_cb, &bwindow, channel[0]);
                show_browser();  //Blocking call; returns when CONTROLLER window is closed
                
                return CHILD;
        }
    }
}


// Forking function -- returns PARENT, CHILD, or 0 for failure
int poll_children()

{
    int i;

    for (i = 1; i < UNRECLAIMED_TAB_COUNTER + 1; i++)
        channel_alive[i] = false;
    
    // Loop through child_pipes and read for new messages to pass on
    // Fork new tab if necessary and return CHILD
    child_req_to_parent* req = malloc(sizeof(child_req_to_parent));
    bool at_least_one_alive;
    
    do
    {
        at_least_one_alive = false;
        
        for (i = 0; i < UNRECLAIMED_TAB_COUNTER + 1; i++) {
            if (channel_alive[i]) {
                at_least_one_alive = true;
                read(channel[i].child_to_parent_fd[READ], req, sizeof(child_req_to_parent));
            }
        }
        
        usleep(1);
    } while (at_least_one_alive);

    return PARENT;
}


int main()

{
    /* //@ marks fork points */
    
	//@ ALL

    switch ( fork_controller() )
    {
        case PARENT:    //@ ROUTER
            poll_children();
            break;
            
        case CHILD:     //@ CONTROLLER
            // At this point CONTROLLER is exiting, so do nothing
            break;
            
        case 0:
        default:
            // Exit if failed
            perror("main: Call to fork_controller() failed");
            return 1;
    }

	return 0;
}

