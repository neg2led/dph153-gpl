
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_awt_MouseInfo__
#define __java_awt_MouseInfo__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace java
  {
    namespace awt
    {
        class MouseInfo;
        class PointerInfo;
      namespace peer
      {
          class MouseInfoPeer;
      }
    }
  }
}

class java::awt::MouseInfo : public ::java::lang::Object
{

public:
  MouseInfo();
  static ::java::awt::PointerInfo * getPointerInfo();
  static jint getNumberOfButtons();
private:
  static ::java::awt::peer::MouseInfoPeer * peer;
public:
  static ::java::lang::Class class$;
};

#endif // __java_awt_MouseInfo__