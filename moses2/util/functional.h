/****
   Copyright 2005-2007, Moshe Looks and Novamente LLC

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
****/

#ifndef _UTIL_FUNCTIONAL_H
#define _UTIL_FUNCTIONAL_H

#include <utility>
#include <functional>

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/call_traits.hpp>
#include <boost/operators.hpp>
#include <boost/utility/result_of.hpp>
#include <boost/utility/addressof.hpp>

#include <ext/functional>

namespace util {
  using __gnu_cxx::select1st;
  using __gnu_cxx::select2nd;
  using __gnu_cxx::identity;


  template<typename T> 
  T* addressof(T& v) { return &v; }
  template<typename T> 
  T& valueof(T* v) { return *v; }

  //a convenience class for manipulating Item that are tagged with & ordered by Tags
  template<typename Item,typename Tag>
  struct tagged_item : public std::pair<Item,Tag>,
    boost::less_than_comparable<tagged_item<Item,Tag> >
  {
    typedef std::pair<Item,Tag> super;
    typedef typename super::first_type first_type;
    typedef typename super::second_type second_type;

    tagged_item(const Item& i,const Tag& s) : super(i,s) { }
    explicit tagged_item(const Item& i) : super(i,Tag()) { }
    tagged_item() { }
    template<class T1, class T2>
    tagged_item(const std::pair<T1,T2>& p) : super(p) { }

    bool operator<(const tagged_item<Item,Tag>& r) const { 
      return this->second<r.second; 
    }

    bool operator<(const Tag& r) const { return this->second<r; }
    bool operator<=(const Tag& r) const { return this->second<=r; }
    bool operator>=(const Tag& r) const { return this->second>=r; }

    operator Item&() { return this->first; }
    operator const Item&() const { return this->first; }
  };
  struct select_item {
    template<typename Item,typename Tag> 
    Item& operator()(tagged_item<Item,Tag>& ti) const { 
      return ti.first; 
    }
    template<typename Item,typename Tag> 
    const Item& operator()(const tagged_item<Item,Tag>& ti) const { 
      return ti.first; 
    }
  };
  struct select_tag {
    template<typename Item,typename Tag> 
    Tag& operator()(tagged_item<Item,Tag>& ti) const { 
      return ti.second; 
    }
    template<typename Item,typename Tag> 
    const Tag& operator()(const tagged_item<Item,Tag>& ti) const { 
      return ti.second;
    }
  };

  //for creating a range of n calls to a generator
  template<typename Generator>
  struct generator_iterator : 
    boost::transform_iterator<Generator,boost::counting_iterator<int> > {
    typedef boost::transform_iterator<Generator,
				      boost::counting_iterator<int> > ti_t;
  

    generator_iterator(int n,Generator gen)
      : ti_t(boost::make_counting_iterator(n),gen) { }
    generator_iterator(int n)
      : ti_t(boost::make_counting_iterator(n)) { }

    typename ti_t::reference dereference() const { 
      return ti_t::functor()();
    }
  };

  template<typename Generator>
  generator_iterator<Generator> begin_generator(Generator gen) {
    return generator_iterator<Generator>(0,gen);
  }
  template<typename Generator>
  generator_iterator<Generator> end_generator(Generator gen,int n) {
    return generator_iterator<Generator>(n,gen);
  }
  
  template<typename It,typename F>
  struct transform_output_iterator :
    boost::output_iterator_helper<transform_output_iterator<It,F> > {

    transform_output_iterator(It it,F f) : _it(it),_f(f) { }

    template<typename T>
    transform_output_iterator& operator=(const T& t) { 
      *_it++=_f(t); 
      return *this;
    }

  private:
    It _it;
    F _f;
  };

  template<typename It,typename F>
  transform_output_iterator<It,F> make_transform_output_iterator(It it,F f) {
    return transform_output_iterator<It,F>(it,f);
  }

  //an output iterator that pushes into a container
  template<typename Container>
  struct push_output_iterator : 
    boost::output_iterator_helper<push_output_iterator<Container> > {
  public:
    push_output_iterator(Container& c) : _c(c) { }

    template<typename T>
    push_output_iterator& operator=(const T& t) { _c.push(t); return *this; }
  private:
    Container& _c;
  };
  template<typename Container>
  push_output_iterator<Container> pusher(Container& c) {
    return   push_output_iterator<Container>(c); 
  }

  //an output iterator that inserts into a container (without a hint)
  template<typename Container>
  struct insert_output_iterator : 
    boost::output_iterator_helper<insert_output_iterator<Container> > {
  public:
    insert_output_iterator(Container& c) : _c(c) { }

    template<typename T>
    insert_output_iterator& operator=(const T& t) { _c.insert(t); return *this; }
  private:
    Container& _c;
  };
  template<typename Container>
  insert_output_iterator<Container> inserter(Container& c) {
    return   insert_output_iterator<Container>(c); 
  }

  template<typename ResultType>
  struct nullary_function {
    typedef ResultType result_type;
  };  

} //~namespace util;

#endif
