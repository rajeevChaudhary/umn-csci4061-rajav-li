// Arbitrary return values for forking functions below
// Must be > 0
#define PARENT 1
#define CHILD 2

// Convenience constants for pipe fd's
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

// + 1 is for controller tab, which takes index 0
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
	if (tab_index < 1 || tab_index > UNRECLAIMED_TAB_COUNTER)
	{
        /*
        char* alert_str;
        sprintf(alert_str, "Please enter a tab index from 1 to %d", UNRECLAIMED_TAB_COUNTER);
		alert(alert_str);
         */
        
        perror("uri_entered_cb: Invalid tab index");
        
        return;
	}
    
	// Get the URL.
	char* uri = get_entered_uri(entry);
    child_req_to_parent new_req;
	
	// Prepare 'request' packet to send to router (/parent) process.
    
    
    // Create a child_req_to_parent with req set to a child_request set to a new_uri_req

	new_req.type = NEW_URI_ENTERED;

	strncpy(new_req.req.uri_req.uri, uri, 511);
	new_req.req.uri_req.uri[511] = '\0';
    
    new_req.req.uri_req.render_in_tab = tab_index;
   
	    // Write that child_req_to_parent to the pipe at channel.child_to_parent_fd[WRITE]
	write(channel.child_to_parent_fd[WRITE], &new_req, sizeof(child_req_to_parent));
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
    
	// Create a new request of type CREATE_TAB
    
    child_req_to_parent new_req;
	//Append your code here

	new_req.type = CREATE_TAB;
	
    // Write the child_req_to_parent to the pipe at channel.child_to_parent_fd[WRITE]
	write(channel.child_to_parent_fd[WRITE], &new_req, sizeof(child_req_to_parent));
}



void controller_flow()
{
    browser_window* bwindow = NULL;
    
    create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb), G_CALLBACK(uri_entered_cb), &bwindow, channel[0]);
    show_browser();  //Blocking call; returns when CONTROLLER window is closed
    
    //Exiting
    printf("Exiting controller\n");
    process_all_gtk_events();
}


void tab_flow(int tab_index)
{
    printf("Starting tab %d \n", tab_index);
    
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
                    printf("New uri\n");
                    render_web_page_in_tab(req.req.uri_req.uri, bwindow);
                    break;
                    
                case TAB_KILLED:
                    printf("Exiting tab %d \n", tab_index);
                    process_all_gtk_events();
                    printf("Exiting tab %d, post \n", tab_index);
                    return;
                    
                case CREATE_TAB:
                default:
                    perror("tab_flow: Message ignored");
                    break;
            }
        else if (errno != EAGAIN)
        {
            printf("Tab die\n");
            process_all_gtk_events();
            return;
        }
    }
    
    printf("SHOULD NOT SEE\n");
}


// Forking function -- returns PARENT, CHILD, or 0 for failure
int fork_tab(int tab_index)

{
    if (pipe(channel[tab_index].parent_to_child_fd) == -1 ||
        pipe(channel[tab_index].child_to_parent_fd) == -1)
    {
        perror("fork_tab: Failed to create one or both pipes");
        return 0;
    }
    else
    {
        channel_alive[tab_index] = true;
        
        switch ( fork() )
        {
            case -1:
                perror("fork_tab: Failed to fork");
                return 0;
                
            case 0:     //@ Parent code (ROUTER)
                close(channel[tab_index].parent_to_child_fd[READ]);
                close(channel[tab_index].child_to_parent_fd[WRITE]);
                                
                fcntl(channel[tab_index].child_to_parent_fd[READ], F_SETFL, O_NONBLOCK);
                
                return PARENT;
                
            default:    //@ Child code (a tab; CONTROLLER or URL_RENDERING_TAB)
                close(channel[tab_index].child_to_parent_fd[READ]);
                close(channel[tab_index].parent_to_child_fd[WRITE]);
                
                fcntl(channel[tab_index].parent_to_child_fd[READ], F_SETFL, O_NONBLOCK);
                
                if (tab_index == 0)
                    controller_flow();
                else
                    tab_flow(tab_index);
                
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
    child_req_to_parent req;
    bool at_least_one_alive;
    int current_new_tab_index = 1;
    
    do
    {
        at_least_one_alive = false;
        
        for (i = 0; i < UNRECLAIMED_TAB_COUNTER + 1; i++)
        {
            if (channel_alive[i])
            {
                at_least_one_alive = true;
                if (read(channel[i].child_to_parent_fd[READ], &req, sizeof(child_req_to_parent)) != -1)
                    switch (req.type) {
                        case CREATE_TAB:
                            printf("Router: CREATE_TAB: %d \n", current_new_tab_index);
                            if (current_new_tab_index <= UNRECLAIMED_TAB_COUNTER)
                            {
                                fork_tab(current_new_tab_index); // This function will set channel_alive[i]
                                current_new_tab_index++;
                            }
                            break;
                            
                        case NEW_URI_ENTERED:
                            printf("Router: NEW_URI_ENTERED: %d \n", req.req.uri_req.render_in_tab);
                            if (channel_alive[req.req.uri_req.render_in_tab])
                                write(channel[req.req.uri_req.render_in_tab].parent_to_child_fd[WRITE], &req, sizeof(child_req_to_parent));
                            break;
                            
                        case TAB_KILLED:
                            printf("Router: TAB_KILLED: %d \n", req.req.uri_req.render_in_tab);
                            write(channel[req.req.uri_req.render_in_tab].parent_to_child_fd[WRITE], &req, sizeof(child_req_to_parent));
                            close(channel[req.req.uri_req.render_in_tab].child_to_parent_fd[READ]);
                            channel_alive[req.req.uri_req.render_in_tab] = false;
                            break;
                            
                        default:
                            break;
                    }
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

    switch ( fork_tab(0) )
    {
        case PARENT:    //@ ROUTER
            poll_children();
            break;
            
        case CHILD:     //@ CONTROLLER
            // At this point a child process is done and exiting, so do nothing
            break;
            
        case 0:
        default:
            // Exit if failed
            perror("main: Call to fork_controller() failed");
            return 1;
    }

	return 0;
}

