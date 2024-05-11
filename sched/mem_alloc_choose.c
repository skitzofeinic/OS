/* =================================
 * Source for mem_alloc.h routines
 * =================================
 * 2024 INTERNAL NOTES:
 * 
 * In the past this was provided as an object file as the code could
 * be used in another assignment (see git:.../scheduler-opgave/scheduler.tex,
 * "Omdat dit een uitwerking is van een andere opgave, die dan wel dit jaar
 * niet wordt gebruikt, wordt deze alleen als object-file aangeboden.")
 * With the increase of platforms, notably, macOS ARM providing a binary
 * is an unnecessary burden. Hence we provide the source code. This is a
 * copy of git:bsc-inf-os/bs2016/collegematerialen-dick/2014/scheduler-testcode/choose.c
 * Both a Makefile and the linux/mem-alloc.o (via readelf) files show 
 * that the previously provided object file was built from this choose.c
 *
 * Arno Bakker, 2024-01-26. Sanitized. See original for comments.
 */
  
/* Voorbeeld uitwerking opgave geheugenbeheer 1993			*/
/*                                                                      */
/* Auteur:	Dick van Albada                                         */
/*		Vakgroep Computersystemen                               */
/*		Universiteit van Amsterdam                              */
/* Datum:	5 oktober 1993						*/
/* Versie:	0.01							*/
/*									*/

#include "mem_alloc.h"

static long *mem_ptr;

#define	ADMIN_SIZE	(2)

void mem_init(long mem[MEM_SIZE])
{
    mem_ptr = mem;
    mem[0] = mem[MEM_SIZE - 1] = -MEM_SIZE;
}

long mem_get(long size)
{
    long index = 0,	/* Het eerste element van het onderzochte blok */
	end,		/* Het laatste element van een toegewezen blok */
	last_free;	/* Het laatste element van het restant */
    long index2 = MEM_SIZE - 1,	/* Voor zoeken vanaf het einde */
    	end2,
    	free2;

    if ((size < 1) || (size > MEM_SIZE - ADMIN_SIZE))
    {
	return (-1);
    }

    while ((index < MEM_SIZE) && (size + ADMIN_SIZE + mem_ptr[index] > 0))
    {
	index = (mem_ptr[index] > 0) ? (index + mem_ptr[index]) :
				       (index - mem_ptr[index]);
    }
    if (index >= MEM_SIZE)
    {
	return (-1);
    }
    while ((index2 > 0) && (size + ADMIN_SIZE + mem_ptr[index2] > 0))
    {
        index2 = (mem_ptr[index2] > 0) ? (index2 - mem_ptr[index2]) :
        				 (index2 + mem_ptr[index2]);
    }
    free2 = index2 + mem_ptr[index2] + 1;
    last_free = index - mem_ptr[index] - 1;
    if ((last_free + free2) < MEM_SIZE)
    {
        end       = index + size + 1;

        /* Als ik alles toewijs, zou end+1 in het volgende blok vallen;
           afblijven dus
           */
        if (last_free > end)
        {
	    mem_ptr[last_free] = mem_ptr[end + 1] =
				mem_ptr[index] + size + ADMIN_SIZE;
        }
        mem_ptr[index] = mem_ptr[end] = size + ADMIN_SIZE;

        /* De aanvrager mag het blok van index+1 t/m end-1 gebruiken. De
           elementen op index en end bevatten administratie van het systeem
           */
        return (index + 1);
    } 
    end2       = index2 - size - 1;

    /* Als ik alles toewijs, zou end2-1 in het volgende blok vallen;
       afblijven dus
       */
    if (free2 < end2)
    {
	mem_ptr[free2] = mem_ptr[end2 - 1] =
				mem_ptr[index2] + size + ADMIN_SIZE;
    }
    mem_ptr[index2] = mem_ptr[end2] = size + ADMIN_SIZE;

    return (end2 + 1);
    
}

void mem_free(long index)
{
    long start, end;

    if ((index < 1) || (index > MEM_SIZE - ADMIN_SIZE))
    {
	return;
    }
    start = index - 1;

    if (mem_ptr[start] < ADMIN_SIZE)
    {
	return;
    }
    end   = start + mem_ptr[start] - 1;

    if ((end >= MEM_SIZE) || (mem_ptr[start] != mem_ptr[end]))
    {
	return;
    }


    mem_ptr[start] = -mem_ptr[start];
    if ((start > 0) && (mem_ptr[start - 1] < 0))
    {
	start += mem_ptr[start - 1];
	mem_ptr[start] -= mem_ptr[end];
    }
    mem_ptr[end] = mem_ptr[start];

    if ((end < MEM_SIZE - 1) && (mem_ptr[end + 1] < 0))
    {
	end -= mem_ptr[end + 1];
	mem_ptr[end] += mem_ptr[start];
        mem_ptr[start] = mem_ptr[end];
    }
}

void mem_available(long *empty, long *large, long *n_hole)
{
    long index = 0, size;

    *empty = 0;
    *large = 0;
    *n_hole = 0;

    while (index < MEM_SIZE)
    {
	if (mem_ptr[index] < 0)
	{
	    size = -mem_ptr[index];
	    *empty += size;
	    *n_hole += 1;
	    if (*large < size)
	    {
		*large = size;
	    }
	    index += size;
	}
	else
	{
	    index += mem_ptr[index];
	}
    }
    *large = (*large > 1) ? (*large - ADMIN_SIZE) : 0;


#ifdef	CORRECT_EMPTY
    *empty = (*empty > 1) ? (*empty - 2) : 0; 
#endif
}

double mem_internal()
{
    long    index = 0, size, n_admin = 0, n_alloc = 0;
    double frag;

    while (index < MEM_SIZE)
    {
	if (mem_ptr[index] < 0)
	{
	    index -= mem_ptr[index];
	}
	else
	{
	    size = mem_ptr[index];
	    n_alloc += size;
	    n_admin += 2;
	    index += size;
	}
    }

    /* Deel niet door nul.
       */
    if (n_alloc <= n_admin)
    {
	return (0.0);
    }
    frag = ((double) n_admin) / ((double) (n_alloc - n_admin));

    return (frag);
}

void mem_exit()
{
    mem_init(mem_ptr);
}
