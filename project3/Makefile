all:		packet_public main_malloc main_mm

packet_public:	mm_public.o packet_public.c packet_public.h
	g++ -g -o packet_public mm_public.o packet_public.c

main_malloc:	mm_public.o main_malloc.c
	g++ -o main_malloc mm_public.o main_malloc.c

main_mm:	mm_public.o main_mm.c
	g++ -o main_mm mm_public.o main_mm.c

mm_public.o:	mm_public.c mm_public.h
	g++ -c mm_public.c

clean:
	rm -f packet_public main_mm main_malloc mm_public.o
