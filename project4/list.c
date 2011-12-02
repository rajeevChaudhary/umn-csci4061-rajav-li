#include <stdio.h>
#include <stdlib.h>


struct list {
	struct elem *sentinel;

	int size;
};

struct elem {
	int data;

	struct elem *prev;
	struct elem *next;
};

struct elem sentinel;
struct list list;



struct elem* list_add(int data) {

	struct elem* e = malloc(sizeof(struct elem));

	e->data = data;

	list.sentinel->prev->next = e;
	e->prev = list.sentinel->prev;
	list.sentinel->prev = e;
	e->next = list.sentinel;

	++list.size;

	return e;
}

void list_remove(struct elem* e) {

	if (e != list.sentinel) {
		e->prev->next = e->next;
		e->next->prev = e->prev;

		--list.size;
	}

}

struct elem* list_shift() {

	struct elem* head = list.sentinel->next;

	if (head != list.sentinel) {
		list_remove(head);
		return head;
	}
	else
		return NULL;
}


void print_list() {

	struct elem* e = list.sentinel->next;
	int i = 0;

	while (e != list.sentinel) {
		++i;
		printf("List element %d: %d\n", i, e->data);
		e = e->next;
	}
}



int main() {

	sentinel.prev = &sentinel;
	sentinel.next = &sentinel;

	list.sentinel = &sentinel;
	list.size = 0;

	list_add(5);
	struct elem* sec = list_add(3);
	list_add(18);

	print_list();

	list_remove(sec);

	print_list();

	struct elem* h = list_shift();

	printf("Shifted: %d\n", h->data);

	print_list();

	printf("List size: %d\n", list.size);

}
