#include "mm_public.h"

/* Return usec */
double comp_time (struct timeval times, struct timeval timee)
{

  double elap = 0.0;

  if (timee.tv_sec > times.tv_sec) {
    elap += (((double)(timee.tv_sec - times.tv_sec -1))*1000000.0);
    elap += timee.tv_usec + (1000000-times.tv_usec);
  }
  else {
    elap = timee.tv_usec - times.tv_usec;
  }
  return ((unsigned long)(elap));

}

int  mm_init (mm_t *MM, int total_size){
    MM->start = malloc(total_size);
    if (MM->start == NULL)
        return -1;
    
    
    // Initialize node list with one node containing the entire space
    MM->chunk_list = (node*) malloc( sizeof(node) );
    MM->chunk_list->address = MM->start;
    MM->chunk_list->size = MM->total_size;
    MM->chunk_list->isFree = true;
    MM->chunk_list->next = MM->chunk_list->prev = NULL;
    
    return 0;
}

void* mm_get (mm_t *MM, int neededSize) {
    // Loop through the node list until we find a node that is:
    // 1) Free, 2) Big enough
    
    node* n = MM->chunk_list;
    
    do {
        if (n->isFree)
            if (n->size >= neededSize)
                break;
    } while ((n = n->next) != NULL);
    
    if (n != NULL) {
    // If the node is just big enough, just set node to "not free"
    // If the node is too big, split the node into two and set the
    // first one to "not free". Add both nodes into the old spot
    // in the list
        
        n->isFree = false;
        
        if (n->size > neededSize) {
            node* right_node = n->next;
            node* new_node = (node*) malloc( sizeof(node) );
            
            new_node->size = n->size - neededSize;
            new_node->address = (unsigned char*)n->address + neededSize;
            new_node->isFree = true;
            new_node->next = right_node;
            new_node->prev = n;
            
            n->size = neededSize;
            n->next = new_node;
            
            if (right_node != NULL)
                right_node->prev = new_node;
        }
    }

    return (n != NULL) ? n->address : NULL;
}

void mm_put (mm_t *MM, void *chunk) {
    // Loop through the node list until we find a node with a pointer
    // that equals the "chunk" pointer
    
    node* n = MM->chunk_list;
    
    do {
        if (n->address == chunk)
            break;
    } while ((n = n->next) != NULL);
    
    if (n != NULL) {
    // If there are free nodes on either side of the node, merge them with
    // this node to make a larger one. In any case, set the node to "free"
        bool left_free = false, right_free = false;
        node* left = n->prev, * right = n->next;
        
        if (left != NULL)
            left_free = left->isFree;
        
        if (right != NULL)
            right_free = right->isFree;
        
        if (left_free && right_free) {
            //All three are free, so make 'left' big enough to
            //cover the space and deallocate the other two
            if (left->next = right->next)
                left->next->prev = left;
            left->size = left->size + n->size + right->size;
            
            free(n);
            free(right);
            
        } else if (left_free) {
            //Just left and n are free
            if (left->next = n->next)
                left->next->prev = left;
            left->size = left->size + n->size;
            
            free(n);
        } else if (right_free) {
            //Just n and right are free
            if (n->next = right->next)
                n->next->prev = n;
            n->size = n->size + right->size;
            
            free(right);
            
            n->isFree = true;
        }
    }
}

void  mm_release (mm_t *MM) {
    free(MM->start);
    
    
    //Deallocate the node list
    node* n = MM->chunk_list;
    node* next = n->next;
    
    do {
        next = n->next;
        free(n);
    } while (n = next);
}
