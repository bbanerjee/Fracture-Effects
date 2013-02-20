#ifndef MATITI_HANDLE_H
#define MATITI_HANDLE_H

namespace Matiti {

  template<class T> class Handle {
  public:
    Handle();
    Handle(T*);
    Handle(const Handle<T>&);
    
    Handle<T>& operator=(const Handle<T>& copy) { return operator=(copy.d_rep); }
    Handle<T>& operator=(T*);
    
    ~Handle();
    
    void detach();
    
    inline const T* operator->() const {
      //ASSERT(d_rep != 0);
      return d_rep;
    }
    inline T* operator->() {
      //ASSERT(d_rep != 0);
      return d_rep;
    }
    inline T* get_rep() {
      return d_rep;
    }
    inline const T* get_rep() const {
      return d_rep;
    }
    inline operator bool() const {
      return d_rep != 0;
    }
    inline bool operator == (const Handle<T>& a) const {
      return a.d_rep == d_rep;
    }
    inline bool operator != (const Handle<T>& a) const {
      return a.d_rep != d_rep;
    }
    inline bool operator == (const T* a) const {
      return a == d_rep;
    }
    inline bool operator != (const T* a) const {
      return a != d_rep;
    }
    inline bool operator == (int a) const {
      //ASSERT(a == 0);
      a=a;     // This quiets the MIPS compilers
      return d_rep == 0;
    }
    inline bool operator != (int a) const {
      ASSERT(a == 0);
      a=a;     // This quiets the MIPS compilers
      return d_rep != 0;
    }
  private:
    T* d_rep;
  }; // end class Handle
  
  template<class T>
  Handle<T>::Handle()
    : d_rep(0)
  {
  }
  
  template<class T>
  Handle<T>::Handle(T* rep)
    : d_rep(rep)
  {
    if(d_rep){
      d_rep->addReference();
    }
  }
  
  template<class T>
  Handle<T>::Handle(const Handle<T>& copy)
    : d_rep(copy.d_rep)
  {
    if(d_rep){
      d_rep->addReference();
    }
  }
  
  template<class T>
  Handle<T>& Handle<T>::operator=(T* copy)
  {
    if(d_rep != copy){    
      if(d_rep){
	if(d_rep->removeReference())
	  delete d_rep;
      }
      d_rep=copy;
      if(d_rep){
	d_rep->addReference();
      }
    }
    return *this;
  }
  
  template<class T>
  Handle<T>::~Handle()
  {
    if(d_rep) {
      if(d_rep->removeReference()) {
        delete d_rep;
      }
    }
  }
  
  template<class T>
  void Handle<T>::detach()
  {
    //ASSERT(d_rep != 0);
    d_rep->lock.lock();
    if(d_rep->ref_cnt==1){
      d_rep->lock.unlock();
      return; // We have the only copy
    }
    T* oldrep=d_rep;
    d_rep=oldrep->clone();
    oldrep->ref_cnt--;
    oldrep->lock.unlock();
    d_rep->ref_cnt++;
  }

} // End namespace Matiti

#endif