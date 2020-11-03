
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __org_w3c_dom_css_CSSPageRule__
#define __org_w3c_dom_css_CSSPageRule__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace org
  {
    namespace w3c
    {
      namespace dom
      {
        namespace css
        {
            class CSSPageRule;
            class CSSRule;
            class CSSStyleDeclaration;
            class CSSStyleSheet;
        }
      }
    }
  }
}

class org::w3c::dom::css::CSSPageRule : public ::java::lang::Object
{

public:
  virtual ::java::lang::String * getSelectorText() = 0;
  virtual void setSelectorText(::java::lang::String *) = 0;
  virtual ::org::w3c::dom::css::CSSStyleDeclaration * getStyle() = 0;
  virtual jshort getType() = 0;
  virtual ::java::lang::String * getCssText() = 0;
  virtual void setCssText(::java::lang::String *) = 0;
  virtual ::org::w3c::dom::css::CSSStyleSheet * getParentStyleSheet() = 0;
  virtual ::org::w3c::dom::css::CSSRule * getParentRule() = 0;
  static ::java::lang::Class class$;
} __attribute__ ((java_interface));

#endif // __org_w3c_dom_css_CSSPageRule__