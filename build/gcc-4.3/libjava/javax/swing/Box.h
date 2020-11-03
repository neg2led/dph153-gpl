
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __javax_swing_Box__
#define __javax_swing_Box__

#pragma interface

#include <javax/swing/JComponent.h>
extern "Java"
{
  namespace java
  {
    namespace awt
    {
        class Component;
        class Dimension;
        class LayoutManager;
    }
  }
  namespace javax
  {
    namespace accessibility
    {
        class AccessibleContext;
    }
    namespace swing
    {
        class Box;
    }
  }
}

class javax::swing::Box : public ::javax::swing::JComponent
{

public:
  Box(jint);
  static ::java::awt::Component * createGlue();
  static ::javax::swing::Box * createHorizontalBox();
  static ::java::awt::Component * createHorizontalGlue();
  static ::java::awt::Component * createHorizontalStrut(jint);
  static ::java::awt::Component * createRigidArea(::java::awt::Dimension *);
  static ::javax::swing::Box * createVerticalBox();
  static ::java::awt::Component * createVerticalGlue();
  static ::java::awt::Component * createVerticalStrut(jint);
  virtual void setLayout(::java::awt::LayoutManager *);
  virtual ::javax::accessibility::AccessibleContext * getAccessibleContext();
private:
  static const jlong serialVersionUID = 1525417495883046342LL;
public:
  static ::java::lang::Class class$;
};

#endif // __javax_swing_Box__