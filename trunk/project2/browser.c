// CSci 4061 Fall '11, Project 2
// Jonathan Rajavuori, Ou Li

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



// Convenience constants for pipe fd's
#define READ 0
#define WRITE 1


// The two global variables below are only used in the ROUTER process

// + 1 is for controller tab, which takes index 0
comm_channel channel[UNRECLAIMED_TAB_COUNTER + 1]; // Pipes
bool channel_alive[UNRECLAIMED_TAB_COUNTER + 1]; // Whether pipes/processes are live






// The following two functions are callbacks registered with the UI
// in the CONTROLLER process.


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
	if (tab_index < 1 || tab_index > UNRECLAIMED_TAB_COUNTER)
	{
        char alert_text[80];
        sprintf(alert_text, "Please enter a tab index from 1 to %d", UNRECLAIMED_TAB_COUNTER);
		alert(alert_text);
        
        return;
	}
    
	// Get the URL.
	char* uri = get_entered_uri(entry);
    child_req_to_parent new_req;

	new_req.type = NEW_URI_ENTERED;

	strncpy(new_req.req.uri_req.uri, uri, 511);
	new_req.req.uri_req.uri[511] = '\0'; // Just in case
    
    new_req.req.uri_req.render_in_tab = tab_index;
   
	if (write(channel.child_to_parent_fd[WRITE], &new_req, sizeof(child_req_to_parent)) == -1)
        perror("[Controller] uri_entered_cb (Write message to pipe failed)");
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
    
	comm_channel channel = ((browser_window*)data)->channel;
    
    child_req_to_parent new_req;
	new_req.type = CREATE_TAB;
	
    // Write the child_req_to_parent to the pipe at channel.child_to_parent_fd[WRITE]
	if (write(channel.child_to_parent_fd[WRITE], &new_req, sizeof(child_req_to_parent)) == -1)
        perror("[Controller] new_tab_created_cb (Write message to pipe failed)");
}





// Flow-control function for tab 0, the controller
void controller_flow()

// NOTES:
// This function never returns, instead calling exit() at all termination points

{
    browser_window* bwindow = NULL;
    
    printf("Controller: Starting\n");
    
    create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb), G_CALLBACK(uri_entered_cb), &bwindow, channel[0]);
    show_browser();  //Blocking call; returns when CONTROLLER window is closed
    
    //Exiting
    printf("Controller: Exiting\n");
    process_all_gtk_events();

    exit(0);
}


// Flow-control function for tabs 1-10
void tab_flow(int tab_index)

// NOTES:
// This function never returns, instead calling exit() at all termination points

{
    printf("Tab %d: Starting\n", tab_index);
    
    child_req_to_parent req;
    browser_window* bwindow = NULL;
    
    create_browser(URL_RENDERING_TAB, tab_index, NULL, NULL, &bwindow, channel[tab_index]);
    
    for (;;)
    {
        process_single_gtk_event();
        usleep(1);
        
        int read_ret = read(channel[tab_index].parent_to_child_fd[READ], &req, sizeof(child_req_to_parent));
        
        if (read_ret > 0)
            switch (req.type) {
                case NEW_URI_ENTERED:
                    printf("Tab %d: Attempting to render %s\n", tab_index, req.req.uri_req.uri);
                    render_web_page_in_tab(req.req.uri_req.uri, bwindow);
                    break;
                    
                case TAB_KILLED:
                    printf("Tab %d: Exiting\n", tab_index);
                    process_all_gtk_events();
                    exit(0);
                    
                case CREATE_TAB:
                default:
                    perror("tab_flow (Invalid message from pipe)");
                    break;
            }
        else if (errno != EAGAIN)
        {
            perror("tab_flow unrecoverable (Problem reading from router pipe)");
            
            process_all_gtk_events();
            exit(1);
        }
    }
    
    // Will not reach this point; infinite loop with internal exit conditions
}



// This function is intended to replace a direct call to fork(),
// handling the pipe initialization and re-routing child flow.
// It is also called for starting CONTROLLER, with index 0.

void fork_tab(int tab_index)

// NOTES:
// Forking function -- returns in the parent, never returns in the child (enters flow function)

{
    if (pipe(channel[tab_index].parent_to_child_fd) == -1 ||
            pipe(channel[tab_index].child_to_parent_fd) == -1)
        perror("fork_tab unrecoverable (Failed to create one or both pipes)");
    else
        switch ( fork() )
        {
            case -1:
                perror("fork_tab unrecoverable (Failed to fork)");
                return;
                
            case 0:     // Parent code (ROUTER)
                channel_alive[tab_index] = true;
                
                close(channel[tab_index].parent_to_child_fd[READ]);
                close(channel[tab_index].child_to_parent_fd[WRITE]);
                                
                fcntl(channel[tab_index].child_to_parent_fd[READ], F_SETFL, O_NONBLOCK);
                
                return;
                
            default:    // Child code (a tab; CONTROLLER or URL_RENDERING_TAB)
                close(channel[tab_index].child_to_parent_fd[READ]);
                close(channel[tab_index].parent_to_child_fd[WRITE]);
                
                fcntl(channel[tab_index].parent_to_child_fd[READ], F_SETFL, O_NONBLOCK);
                
                if (tab_index == 0)
                    controller_flow(); //flow-control function; does not return, calls exit() when done
                else
                    tab_flow(tab_index); //flow-control function; does not return, calls exit() when done
                
                // This point should never be reached
                exit(2);
        }
}



// This function only runs in the ROUTER process, and it comprises most of its flow.
// The purpose of this function is to continually poll all of the pipes in the
// channel[] array for new messages from CONTROLLER or a URL_RENDERING_TAB.
// It returns when all of the channels are dead.

void poll_children()

// NOTES:
// Forking function -- returns in the parent, never returns in the child (enters flow function through fork_tab())

{
    int i;

    // Initialize global bool array channel_alive,
    // sister array to channel[] to keep track
    // of child process and pipe status
    for (i = 1; i < UNRECLAIMED_TAB_COUNTER + 1; i++)
        channel_alive[i] = false;
    
    
    // Loop through child_pipes and read for new messages to pass on
    // Fork new tab if necessary
    child_req_to_parent req;
    bool at_least_one_alive; // Loop exit condition
    int current_new_tab_index = 1;
    
    do
    {
        at_least_one_alive = false; // We want to exit!
        
        for (i = 0; i < UNRECLAIMED_TAB_COUNTER + 1; i++)
        {
            // Note that this checks the CONTROLLER pipe, too.
            // However, if CONTROLLER dies, we kill everything
            if (channel_alive[i])
            {
                at_least_one_alive = true; // ... but not just yet, we still have children
                
                if (read(channel[i].child_to_parent_fd[READ], &req, sizeof(child_req_to_parent)) != -1)
                    switch (req.type) {
                        case CREATE_TAB:
                            printf("Router: CREATE_TAB: %d \n", current_new_tab_index);
                            if (current_new_tab_index <= UNRECLAIMED_TAB_COUNTER)
                            {
                                fork_tab(current_new_tab_index); // This function will set channel_alive[i]
                                current_new_tab_index++;
                            }
                            else
                                printf("There are already %d tabs!\n", UNRECLAIMED_TAB_COUNTER);
                            break;
                            
                        case NEW_URI_ENTERED:
                            printf("Router: NEW_URI_ENTERED: %d \n", req.req.uri_req.render_in_tab);
                            if (channel_alive[req.req.uri_req.render_in_tab])
                                write(channel[req.req.uri_req.render_in_tab].parent_to_child_fd[WRITE], &req, sizeof(child_req_to_parent));
                            else
                                printf("Tab %d does not exist!\n", req.req.uri_req.render_in_tab);
                            break;
                            
                        case TAB_KILLED:
                            printf("Router: TAB_KILLED: %d \n", req.req.killed_req.tab_index);
                            write(channel[req.req.killed_req.tab_index].parent_to_child_fd[WRITE], &req, sizeof(child_req_to_parent));
                            close(channel[req.req.killed_req.tab_index].child_to_parent_fd[READ]);
                            channel_alive[req.req.killed_req.tab_index] = false;
                            
                            // If CONTROLLER is dying, kill everything.
                            // (This isn't strictly necessary, but it's the behavior of the solution binary provided)
                            if (req.req.killed_req.tab_index == 0)
                            {
                                printf("Router: Controller killed - Killing all rendering tabs.\n");
                                
                                for (i = 1; i < UNRECLAIMED_TAB_COUNTER + 1; i++)
                                {
                                    if (channel_alive[i])
                                    {
                                        // Recycles the kill request
                                        
                                        // We don't actually have to set the index, since the child ignores it,
                                        // but it might cause problems later on if it's changed to not ignore.
                                        req.req.killed_req.tab_index = i;
                                        
                                        printf("Router: Killing tab: %d \n", i);
                                        write(channel[i].parent_to_child_fd[WRITE], &req, sizeof(child_req_to_parent));
                                        close(channel[i].child_to_parent_fd[READ]);
                                        channel_alive[i] = false;
                                        
                                    }
                                }
                                
                                return;
                            }
                            
                            break;
                            
                        default:
                            perror("poll_children (Read a message of invalid type)");
                            break;
                    }
            }
        }
        
        usleep(1);
    } while (at_least_one_alive);
    
    return;
}


int main()

{
    if (UNRECLAIMED_TAB_COUNTER <= 1) // Sanity check
    {
        perror("UNRECLAIMED_TAB_COUNTER too low");
        return 1;
    }
    
    fork_tab(0); // Fork controller
    poll_children(); // Enter polling loop -- blocks until all tabs and controller are killed
    
    
    //Exiting
    return 0;
}

