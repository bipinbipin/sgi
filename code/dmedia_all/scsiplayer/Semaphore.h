/*
 * Semaphore.h
 */
 
#ifndef _Semaphore_h
#define _Semaphore_h

#include <ulocks.h>

class Semaphore {
    public:
	Semaphore(int);
	~Semaphore();

	void usv();
    	void usp();
	void usinit(int);
	
    private:
	static 	usptr_t	    *_arena;
	usema_t		    *_sema;
	
};

#endif	// _Semaphore_h

