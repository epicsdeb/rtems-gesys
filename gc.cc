/* $Id: gc.cc,v 1.2 2007/02/01 04:36:42 strauman Exp $ */
#include <rtems++/rtemsTask.h>
#include <rtems++/rtemsMessageQueue.h>

/* Author: Till Straumann, <strauman@slac.stanford.edu>, 2003/10/8 */

/* This file implements a workaround for a problem
 * related to
 *    PR#504: "libc allocator must not be used
 *            from thread-dispatching disabled 
 *            code"
 *
 * The problem is that the RTEMS task variable destructor
 * is run in such a context and existing (RTEMS and custom)
 * code calls 'free()' from their destructor :-(
 *
 * This workaround re-implements 'free()':
 *
 * if _Thread_Dispatch_disable_level > 0, the
 * 'arg' to 'free(arg)' is deposited in a
 * mailbox from where it is picked up by a
 * low priority thread executing the 'real'
 * 'free()' in its context where it is safe.
 *
 * We use the C++ API to save typing and to
 * use C++ static constructors for transparent
 * initialization...
 *
 * NOTE: this doesn't need to be patched into RTEMS
 *       but may be wrapped by the GNU linker using
 *       the --wrap option (add "-Wl,--wrap,free" to the
 *       application LDFLAGS).
 *       If 'cexp' is used for run-time loading, the
 *       system symbol table has to be rearranged:
 *
 *       First, rearrange the executable's symbol table
 *
 *         $(OBJCOPY) --redefine-sym free=__real_free \ 
 *                    --redefine-sym __wrap_free=free <executable ELF file>
 *
 *       Then, extract the symbol table as usual
 *
 *         $(XSYMS)  <executable ELF file> <symbol table file>
 */

#undef DEBUG

/* a GC task object (derived from rtemsTask) */
class rtemsGCHack : public rtemsTask {
public:
	/* default constructor: creates a task and mailbox and then
     * starts the task
	 */
	rtemsGCHack()
		:rtemsTask("GCHk", 200, RTEMS_MINIMUM_STACK_SIZE),
		 q("GCHk", 5, sizeof(void*))
		{ this->start(0); }

	/* send a request */
	void requestFree(void *ptr)
		{ this->q.send(static_cast<void*>(&ptr), sizeof(ptr)); };
protected:
	/* we have to implement the body of our task, see below */
	virtual void rtemsGCHack::body(rtems_task_argument unused);
private:
	/* a small message queue to pass requests from dispatch-disabled
	 * sections to the GC thread
	 */
	rtemsMessageQueue q;
};

extern "C" void __real_free(void*);

#ifdef DEBUG
extern "C" void printk(const char *,...);
#endif

/* The GC task body */
void rtemsGCHack::body(rtems_task_argument unused)
{
void             *ptr;
uint32_t s = sizeof(ptr);

#ifdef DEBUG
	printk("GC thread started...\n");
#endif

	while (1) {
		/* wait for a request */
		this->q.receive(static_cast<void*>(&ptr),s);
#ifdef DEBUG
		printk("GC thread freeing 0x%x\n",ptr);
#endif
		/* and do the real free() here */
		__real_free(ptr);
	}
}

/* a single object; is transparently and automatically initialized :-) */
static rtemsGCHack theHack;

/* this replaces the LIBC 'free' */
extern "C" void __wrap_free(void *arg)
{
	if (_Thread_Dispatch_disable_level > 0) {
		/* if they call us from a dispatch disabled section, just
		 * post a request to the GC task and return
		 */
		theHack.requestFree(arg);
	} else {
		/* otherwise, proceed as usual */
		__real_free(arg);
	}
}
