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

#include <iostream>

#include "combo/tree_generation.h"
#include "util/selection.h"
#include <boost/lexical_cast.hpp>

int main(int argc,char** argv) {
  using namespace util;
  using namespace trees;
  using namespace boost;
  using namespace std;

  int max_depth;
  int noe; //number of expressions to generate
  bool tce; //type check enabled
  NodeSelector<string> ss;

  if(argc!=4) {
    cout << "Usage :" << endl <<
      "tree_gen [bool|real|mix] max_depth number_of_expressions" << endl;
    exit(1);
  }

  max_depth = lexical_cast<int>(argv[2]);
  noe = lexical_cast<int>(argv[3]);
  tce = string(argv[1])==string("mix");

  if(string(argv[1])==string("bool") || string(argv[1])==string("mix")) {
    ss.add("and",2,100);
    ss.add("or",2,100);
    ss.add("not",1,100);
  }
  if(string(argv[1])==string("real") || string(argv[1])==string("mix")) {
    //second arg is arity, third is (relative) selection likelihood
    ss.add("+",2,100);
    ss.add("*",2,100);
    ss.add("/",2,100);

    ss.add("log",1,100);
    ss.add("exp",1,100);
    ss.add("sin",1,100);

    ss.add("1",0,100);
    ss.add("0",0,100);

    for (int i=0;i<200;++i)
      ss.add(lexical_cast<string>(randfloat()),0,1);
  }
  if(string(argv[1])==string("mix")) {
    ss.add("contin_if",3,100);
    ss.add("0<",1,100);
    ss.add("impulse",1,100);
  }
  ss.add("#1",0,100);
  ss.add("#2",0,100);
  ss.add("#3",0,100);
  ss.add("#4",0,100);
  ss.add("#5",0,100);

  vector<tree<string> > trees(noe);
  ramped_half_and_half(trees.begin(),trees.end(),ss,2,max_depth,tce);
  for (vector<tree<string> >::const_iterator tr=trees.begin();
       tr!=trees.end();++tr)
    cout << (*tr) << endl;
}
