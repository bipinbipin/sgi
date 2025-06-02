/*
 * Semaphore.c++
 * 
 */

#include "Semaphore.h"

//#define DBG(x) {x;}
#define DBG(x) {}  

usptr_t *Semaphore::_arena = NULL;

Semaphore::Semaphore(int flag) {
  if(_arena == NULL) {
    ::usconfig(CONF_ARENATYPE, US_SHAREDONLY);    
    _arena = ::usinit(tmpnam(NULL)); 
  }
  _sema = ::usnewsema(_arena, flag);
}

Semaphore::~Semaphore() {
    ::usfreesema(_sema, _arena);
}

void Semaphore::usv() {
  DBG(fprintf(stderr,"Semphore before release = %d\n", ustestsema(_sema)));
  ::usvsema(_sema);
  DBG(fprintf(stderr,"Releasing Semphore; Semphore = %d\n", ustestsema(_sema)));
}

void Semaphore::usp() {
  DBG(fprintf(stderr,"Semphore before lock = %d\n", ustestsema(_sema)));
  ::uspsema(_sema);
  DBG(fprintf(stderr,"Locking Semphore; Semphore = %d\n", ustestsema(_sema)));
}

void Semaphore::usinit(int flag) {
  ::usinitsema(_sema, flag);
}
