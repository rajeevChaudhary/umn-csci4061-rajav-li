#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// MurmurHash 2 algorithm created by Austin Appleby. Code is in the public domain.
// (Current version is MurmurHash 3; This version is simpler.)
uint32_t MurmurHash2 ( const void * key, int len, uint32_t seed )
{
  // 'm' and 'r' are mixing constants generated offline.
  // They're not really 'magic', they just happen to work well.

  const uint32_t m = 0x5bd1e995;
  const int r = 24;

  // Initialize the hash to a 'random' value

  uint32_t h = seed ^ len;

  // Mix 4 bytes at a time into the hash

  const unsigned char * data = (const unsigned char *)key;

  while(len >= 4)
  {
    uint32_t k = *(uint32_t*)data;

    k *= m;
    k ^= k >> r;
    k *= m;

    h *= m;
    h ^= k;

    data += 4;
    len -= 4;
  }

  // Handle the last few bytes of the input array

  switch(len)
  {
  case 3: h ^= data[2] << 16;
  case 2: h ^= data[1] << 8;
  case 1: h ^= data[0];
      h *= m;
  };

  // Do a few final mixes of the hash to ensure the last few
  // bytes are well-incorporated.

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;

  return h;
}






struct elem* hash_table[100];

struct list {
	struct elem *sentinel;

	int size;
};

struct elem {
	char data[1024];

	struct elem *prev;
	struct elem *next;

	struct elem *next_collision;
	struct elem *prev_collision;
};

struct elem sentinel;
struct list list;



void hash_add(struct elem* e) {
	e->next_collision = NULL;
	e->prev_collision = NULL;

	unsigned hash = MurmurHash2(e->data, 1024, 436) % 100;

	if (hash_table[hash] == NULL) {
		hash_table[hash] = e;
	} else {
		struct elem* current = hash_table[hash];
		while (current->next_collision != NULL)
			current = current->next_collision;
		current->next_collision = e;
		e->prev_collision = current;
	}
}

struct elem* list_add(const char* const data) {

	struct elem* e = malloc(sizeof(struct elem));

	strncpy(e->data, data, 1024);

	list.sentinel->prev->next = e;
	e->prev = list.sentinel->prev;
	list.sentinel->prev = e;
	e->next = list.sentinel;

	++list.size;

	hash_add(e);

	return e;
}

void hash_remove(struct elem* e) {

	if (e->prev_collision == NULL) {
		unsigned hash = MurmurHash2(e->data, 1024, 436) % 100;

		if (e->next_collision == NULL) // This hash is now empty
			hash_table[hash] = NULL;
		else
			hash_table[hash] = e->next_collision;
	} else {
		e->prev_collision->next_collision = e->next_collision;
		if (e->next_collision != NULL)
			e->next_collision->prev_collision = e->prev_collision;
	}

}

void list_remove(struct elem* e) {

	if (e != list.sentinel) {
		e->prev->next = e->next;
		e->next->prev = e->prev;

		--list.size;
	}

	hash_remove(e);

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
		printf("List element %d: %s, hash: %d\n", i, e->data, MurmurHash2(e->data, 1024, 436) % 100);
		e = e->next;
	}
}



void print_hash_table() {

	struct elem* e;

	int i;
	for (i = 0; i < 100; ++i) {
		printf("Entering hash %d\n", i);
		e = hash_table[i];
		while (e != NULL) {
			printf("Element: %s\n", e->data);
			e = e->next_collision;
		}
	}

}




struct elem* hash_search(const char* const data) {

	unsigned hash = MurmurHash2(data, 1024, 436) % 100;

	return hash_table[hash];
}




int main () {

	int i;
	for (i = 0; i < 100; ++i) {
		hash_table[i] = NULL;
	}



	const char* s1 = "First element";
	const char* s2 = "Second element";
	const char* s3 = "Third element";


	sentinel.prev = &sentinel;
	sentinel.next = &sentinel;

	list.sentinel = &sentinel;
	list.size = 0;



	struct elem* e1 = list_add(s1);
	list_add(s2);
	struct elem* e3 = list_add(s3);
	list_add(s1);

	print_list();


	list_remove(e3);

	print_list();

	list_remove(e1);

	print_list();
	print_hash_table();

    char key[1024];

    strncpy(key, s1, 1024);

	struct elem* e = hash_search(key);

	printf("Search result: %s\n", e->data);


}
