/****************************************************************************
   Deze header file bevat de voor de opgave over high-level scheduling en
   geheugen-allocatie benodigde definities

   Auteur:      Dick van Albada
                Vakgroep Computersystemen
                Kruislaan 403
   Datum:       7 septemebr 2003
   Versie:      0.3
****************************************************************************/
#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <assert.h>

/****************************************************************************
   De verschillende soorten events:
   NEW_PROCESS_EVENT - er is een nieuw proces in de new_proc rij
        bijgeplaatst.
   TIME_EVENT - het lopende proces heeft zijn time-slice opgebruikt.
        Wordt niet gegenereerd, tenzij je zelf "set_slice" aanroept.
        Hoort b.v. in een Round-Robin schedule thuis.
   READY_EVENT - een proces is klaar met I/O en is weer achteraan de
        ready_proc rij geplaatst. Voor sommige CPU-scheduling algoritmen
        een punt om weer een beslissing te nemen.
   IO_EVENT - het lopende proces gaat I/O doen en is achteraan de
        io_proc rij geplaatst. Als je de volgorde van de processen in
        de io_proc queue of in de ready queue wil aanpassen, kan dat nu.
   FINISH_EVENT - het lopende proces is beeindigd en in de defunct_proc rij
        geplaatst. Een goede gelegenheid om nieuwe processen toe te laten.
****************************************************************************/

typedef enum {
    NEW_PROCESS_EVENT,
    TIME_EVENT,
    READY_EVENT,
    IO_EVENT,
    FINISH_EVENT
} event_type;

/****************************************************************************
   de structuur "pcb" bevat alle informatie die voor de scheduler
   beschikbaar is.
   SIM_pcb verwijst naar de voor de simulator benodigde informatie en mag
           niet worden gewijzigd.
   userdata wordt door de simulator niet gebruikt (aanvankelijk NULL).
           Deze pointer is beschikbaar om desgewenst voor de scheduler een
           eigen administratie aan de pcb te kunnen koppelen.
   prev en next worden gebruikt voor het construeren van dubbel verbonden
           lijsten. Ze kunnen zowel door de simulator als door de scheduler
           worden gewijzigd.
   mem_need bevat het aantal voor dit proces benodigde longs geheugen.
           mem_need wordt door de simulator ingevuld en mag niet worden
           gewijzigd.
   mem_base staat aanvankelijk op -1 en moet door de (hoog-niveau) scheduler
           een maal worden gevuld met de begin-locatie in het te gebruiken
           geheugen-array. Door jouw scheduler een maal te veranderen.
****************************************************************************/

typedef struct student_pcb {
    void *sim_pcb;
    void *userdata;
    struct student_pcb *prev, *next;
    long mem_need, mem_base;
} student_pcb;

// The functions below are convenient to use for process queue manipulation.
// Feel free to implement more of your own
// functions if these functions do not do what you want.

// Returns the length of a queue
static int queue_length(student_pcb **queue) {
    int length = 0;

    student_pcb *current = *queue;
    while (current) {
        ++length;
        current = current->next;
    }

    return length;
}

// Attaches a new item at the front of a queue (item must not be in any queue,
// i.e. newly created or removed)
static void queue_prepend(student_pcb **queue, student_pcb *item) {
    assert(item->prev == NULL);
    assert(item->next == NULL);

    item->next = *queue;
    if (*queue) {
        (*queue)->prev = item;
    }
    *queue = item;
    item->prev = NULL;
}

// Retrieves the last item of a queue
static student_pcb *queue_last(student_pcb **queue) {
    student_pcb *current = *queue;
    student_pcb *last = NULL;

    while (current) {
        last = current;
        current = current->next;
    }

    return last;
}

// Removes an item from a queue (THIS FUNCTION DOES NOT CHECK WHETHER THE ITEM
// IS IN THE QUEUE AND WILL NOT BEHAVE PROPERLY WHEN THE WRONG QUEUE IS PASSED)
static void queue_remove(student_pcb **queue, student_pcb *item) {
    student_pcb *next = item->next;

    // Fix the next pointer of the node before this (or the head of the list if
    // there is no previous)
    if (item == *queue) {
        assert(item->prev == NULL);
        *queue = next;
    } else {
        item->prev->next = next;
    }

    // Fix the previous pointer of the node after this
    if (item->next) {
        next->prev = item->prev;
    }

    item->next = NULL;
    item->prev = NULL;
}

// Append an item at the back of the queue (item must not be in any queue, i.e.
// newly created or removed)
static void queue_append(student_pcb **queue, student_pcb *item) {
    assert(item->next == NULL);
    assert(item->prev == NULL);

    if (*queue) {
        // Queue is non-empty, find the last node and attach item to it
        student_pcb *last = queue_last(queue);
        assert(last->next == NULL);
        last->next = item;
        item->prev = last;
    } else {
        // Queue is empty
        *queue = item;
    }
}

/****************************************************************************
   De wachtrijen.
   Nieuwe processen worden achteraan in de rij new_proc bijgeplaatst.
        (programma practicumleiding)
   Na toewijzing van geheugen dienen ze in de ready_proc rij te worden
   geplaatst.
        (procedure schedule)
   Een proces wordt voor de CPU gescheduled door het voorin de ready_proc
   rij te plaatsen. Zonder enige verdere voorzieningen werken de io_proc
   en ready_proc rijen op FCFS basis hun klanten af.
        (procedure schedule - een andere CPU scheduling dan FCFS vereist
        dus aanpassingen in de volgorde van processen in deze queues.
   Als een proces I/O wil doen (gesimuleerd), komt het in de io_proc rij.
   Laat deze rij met rust.
   Een beeindigd proces komt in de defunct_proc rij. Ruim deze op.
 *****************************************************************************/

extern student_pcb *new_proc, *ready_proc, *io_proc, *defunct_proc;

/****************************************************************************
   De door de practicum-leiding aangeleverde fucties
*****************************************************************************/

double sim_time();

/****************************************************************************
   sim_time geeft de gesimuleerde "wall-clock time" terug. Gebruik
   naar het je goeddunkt
*****************************************************************************/

extern void set_slice(double slice);

/****************************************************************************
   set_slice zorgt dat over slice tijdseenheden een TIME_EVENT optreedt.
   Er kan maar een TIME_EVENT tegelijk in de pijp zitten, dus iedere
   set_slice aanroep "overschrijft" de vorige.
   set_slice zorgt er intern voor dat slice steeds minstens 1.0 is, om
   voortgang te garanderen.
   Bij een TIME_EVENT wordt voordat de scheduler wordt aangeroepen steeds
   een set_slice(9.9e12) gedaan om te voorkomen dat de simulator daarop kan
   blijven hangen.
*****************************************************************************/

long rm_process(student_pcb **proces);

/****************************************************************************
   rm_process heeft twee taken:
   1. het verzamelt de nodige statistische gegevens over de executie
      van het proces voor het "eindrapport"
   2. het ruimt de pcb en de "SIM_pcb" op en werkt de defunct_proc rij bij.
   rm_process ruimt de eventueel door "your_admin" gebruikte ruimte
   niet op en geeft ook het gereserveerde geheugen niet vrij. Dat
   moet je bij een "FINISH_EVENT" zelf doen.
****************************************************************************/

/***************************************************************************
   De functie variabele finale wordt door het hoofdprogramma geinitialiseerd
   met een vrijwel lege functie die alleen de tekst "Einde van het programma"
   afdrukt.
   Hij wordt aangeroepen vlak voor het eind van het programma, als de
   door het hoofdprogramma verzamelde statistische gegevens zijn afgedrukt.
   Door finale naar een eigen functie van het juiste type te laten verwijzen,
   kun je ervoor zorgen dat je eigen afsluitende routine wordt aangeroepen
   om zo eventuele eigen statistieken af te drukken.
   Een interessante mogelijkheid is b.v. de samenhang tussen de wachttijd op
   geheugen en de grootte van het aangevraagde geheugen te onderzoeken.
   Treedt er starvation op, en zo ja, voor welke processen?
****************************************************************************/

typedef void function();

extern function *finale;

/****************************************************************************
   Voor de eigenlijke meting worden 100 processen opgestart om de
   queues te vullen. Daarna worden de statistieken weer op nul gezet.
   Definieer zelf een functie waar je reset_stats naar laat wijzen
   om dat ook eventueel voor je eigen statistieken te doen.
****************************************************************************e */

extern function *reset_stats;

/****************************************************************************
   De door de practicanten te schrijven routine
****************************************************************************e */

void schedule(event_type event);

#endif /* SCHEDULE_H */
