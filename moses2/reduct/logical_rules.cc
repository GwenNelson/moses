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

#include "reduct/logical_rules.h"
#include "util/algorithm.h"
#include "util/foreach.h"
#include "combo/assumption.h"

namespace reduct {
  typedef vtree::sibling_iterator sib_it;
  typedef vtree::iterator pre_it;

  //ensure that all arguments and or nodes have an and node as their parent.
  //this is important for other normalizations to be able to catch all cases
  void insert_ands::operator()(vtree& tr,vtree::iterator it) const {
    if ((is_argument(*it) || *it==id::logical_or) &&
	(!tr.is_valid(tr.parent(it)) || *tr.parent(it)!=id::logical_and)) {
      tr.prepend_child(it,*it);
      *it=id::logical_and;
      tr.reparent(it.begin(),++it.begin(),it.end());
    }
  }

  void remove_unary_junctors::operator()(vtree& tr,vtree::iterator it) const {
    if ((*it==id::logical_and || *it==id::logical_or) && it.has_one_child()) {
      *it=*it.begin();
      tr.erase(tr.flatten(it.begin()));
    }
  }

  void remove_dangling_junctors::operator()(vtree& tr,vtree::iterator it) const {
    if (*it==id::logical_and || *it==id::logical_or)
      for (sib_it sib=it.begin();sib!=it.end();)
	if ((*sib==id::logical_and || *sib==id::logical_or) && sib.is_childless())
	  tr.erase(sib++);
	else
	  ++sib;
  }

  //and(true X)->X, or(true X)->true
  //and(false X)->false, or(false X)->X
  //or(X)->X
  //and(or X) -> and(X), or(and X) -> or(X)
  void eval_logical_identities::operator()(vtree& tr,vtree::iterator it) const {
    if (*it==id::logical_and) {
      for (sib_it sib=it.begin();sib!=it.end();) {
	if ((*sib==id::logical_or && sib.is_childless()) ||
	    *sib==id::logical_true) {
	  sib=tr.erase(sib);
	} else if (*sib==id::logical_false) {
	  *it=*sib;
	  tr.erase_children(it);
	  break;
	} else {
	  ++sib;
	}
      }
    } else if (*it==id::logical_or) {
      if (it.has_one_child()) {
	*it=*it.begin();
	tr.erase(tr.flatten(it.begin()));
      } else {
	for (sib_it sib=it.begin();sib!=it.end();) {
	  if ((*sib==id::logical_and && sib.is_childless()) ||
	      *sib==id::logical_false) {
	    sib=tr.erase(sib);
	  } else if (*sib==id::logical_true) {
	    *it=*sib;
	    tr.erase_children(it);
	    break;
	  } else {
	    ++sib;
	  }
	}
      }
    }
  }

  //!!a->a,!(a&&b)->(!a||!b),!(a||b)->(!a&&!b),
  void reduce_nots::operator()(vtree& tr,vtree::iterator it) const {
    if (*it==id::logical_not) {
      if (*it.begin()==id::logical_not) {
	//!!a->!a
	tr.erase(tr.flatten(it.begin()));
	//!a->a
	*it=*it.begin();
	tr.erase(tr.flatten(it.begin()));
	operator()(tr,it);
      } else if (*it.begin()==id::logical_and || 
		 *it.begin()==id::logical_or) {
	//!(a&&b) -> (!a||!b), and !(a||b) -> (!a&&!b)
	//first transform to comp_op(a,b)
	tr.flatten(it.begin());
	*it=(*it.begin()==id::logical_and ? 
	     id::logical_or : 
	     id::logical_and);
	tr.erase(it.begin());
	//now to comp_op(!a,!b)
	for (sib_it sib=it.begin();
	     sib!=it.end();++sib)
	  sib=tr.insert_above(sib,id::logical_not);
      } else if (is_argument(*it.begin())) {
	*it=*it.begin();
	tr.erase_children(it);
	get_argument(*it).negate();
      }
    }
  }

  
  //and(x_1 x_2 x_3 ...)
  //reduce x_1 assuming x_2, x_3 ...
  //reduce x_2 assuming x_1, x_3 ...
  //...
  //and choose the one that shorten the most the expression
  void reduce_and_assumptions::operator()(vtree& tr,vtree::iterator it) const {
    int best_diff = 0;
    int best_index = -1;
    vtree best_tree;
    if(*it==id::logical_and) {
      for(sib_it sib = it.begin(); sib != it.end(); ++sib) {
	//copy the tree to reduce
	vtree copy_tr = tr.subtree(sib, tr.next_sibling(sib));
	//copy old assumptions
	sib_it bna = copy_tr.begin(); //before new assumption
	for(sib_it a = tr.next_sibling(tr.begin()); a != tr.end(); ++a)
	  bna = copy_tr.insert_subtree_after(bna, a);
	//add new assumptions
	for(sib_it a = it.begin(); a != it.end(); ++a) {
	  if(a!=sib)
	    insert_assumption(copy_tr, pre_it(a));
	}
	//subtree_size on copy_tr.begin() to not add size of assumptions
	int s_before = copy_tr.subtree_size(copy_tr.begin());
	(*_reduction)(copy_tr);
	//subtree_size on copy_tr.begin() to not add size of assumptions
	int s_after = copy_tr.subtree_size(copy_tr.begin());
	int diff = s_after - s_before;
	if(diff < best_diff) {
	  best_diff = diff;
	  best_index = tr.sibling_index(sib);
	  best_tree = copy_tr;
	}
      }
      //replace by the best reduction if there is
      if(best_index>-1)
	tr.replace(tr.child(it, best_index), best_tree.begin());
    }
  }

  //reduce or(x_1 x_2 x_3 ...) by reducing assuming not(and(not(x_1)...))
  //using the reduce_and_assumptions
  void reduce_or_assumptions::operator()(vtree& tr,vtree::iterator it) const {
    if(*it==id::logical_or) {
      assert(!it.is_childless());
      *it = id::logical_not;
      pre_it new_or = tr.prepend_child(it, id::logical_or);
      tr.reparent(new_or, tr.next_sibling(it.begin()), it.end());
      pre_it first_not = tr.wrap(new_or, id::logical_not);
      (*_reduction)(tr, first_not);
      //to avoid an infinit recursion not(and(x y...) is reduced to or(not(x) ...)
      //then all not(x), not(y) ... are reduced
      reduce_nots()(tr, it);
      for(sib_it sib = it.begin(); sib != it.end(); ++sib) {
	(*_reduction)(tr, pre_it(sib));
      }
    }
  }

  //heuristic reduction of ORs based on complementary pairs:
  // 1) pairwise implications of conjuncts (X&&a)||((Y&&!a) impl  X&&Y
  // 2) for all pairs of conjuncts, including implications (in X)
  //    if X is a subset of (or equal to) Y, remove Y
  // Also, true||X -> true, false||X -> X
  void reduce_ors::operator()(vtree& tr,vtree::iterator it) const {
    if (*it!=id::logical_or)
      return;

    //first construct a mapping between items and ints,
    //and convert all conjunctions to this format
    nf_mapper<vertex> mapper;
    nf f(mapper.add_dnf(tr.begin(it),tr.end(it)));
    int sz=number_of_literals(f);

    //before this method will work, need to eliminate A&&!A clauses
    f.remove_if(bind(tautology,_1));
      
    //remove clauses which are subsets of others
    pairwise_erase_if(f,bind(subset_eq,_1,_2));

    //create implications (and remove subsets) we could be cleverer by trying
    //implications with implications, and/or not updating right a way, but it
    //makes things more complicated and in practice doesn't seem to lead to
    //appreciable further reduction
    for (nf::iterator c1=f.begin();c1!=f.end();++c1) {
      for (nf::iterator c2=f.begin();c2!=c1;++c2) {
	nf impls;
	implications(*c1,*c2,std::back_inserter(impls));

	for (nf::const_iterator impl=impls.begin();
	     impl!=impls.end();++impl) {
	  for (nf::iterator c=f.begin();c!=f.end();)
	    if (c!=c1 && c!=c2 && subset_eq(*impl,*c))
	      c=f.erase(c);
	    else
	      ++c;
	  if (subset(*impl,*c1))
	    *c1=*impl;
	  if (subset(*impl,*c2))
	    *c2=*impl;
	}
      }
    }

    if (sz!=number_of_literals(f))
      mapper.extract_dnf(f.begin(),f.end(),tr,it);
  }

  //heuristic reduction of ANDs based on complementary pairs:
  // 1) pairwise implications of conjuncts (X||a)&&((Y||!a) impl (X||Y)
  // 2) for all pairs of conjuncts, including implications (in X)
  //    if X is a subset of (or equal to) Y, remove X
  // Also, true&&X -> X, false&&X -> false
  void reduce_ands::operator()(vtree& tr,vtree::iterator it) const {
    if (*it!=id::logical_and)
      return;

    //first construct a mapping between items and ints,
    //and convert all conjunctions to this format
    nf_mapper<vertex> mapper;
    nf f(mapper.add_cnf(tr.begin(it),tr.end(it)));
    int sz=number_of_literals(f);

    //negate
    for (nf::iterator c=f.begin();c!=f.end();++c) {
      clause tmp;
      std::transform(c->begin(),c->end(),std::inserter(tmp,tmp.begin()),
		     std::negate<int>());
      *c=tmp;
    }

    //before this method will work, need to eliminate A&&!A clauses
    f.remove_if(bind(tautology,_1));

    if (f.empty()) { //tautological expression
      tr.erase_children(it);
      *it=id::logical_true;
      return;
    }

    //remove clauses which are supersets of others
    pairwise_erase_if(f,bind(subset_eq,_1,_2));

    //create implications (and remove subsets)
    for (nf::iterator c1=f.begin();c1!=f.end();++c1) {
      for (nf::iterator c2=f.begin();c2!=c1;++c2) {
	nf impls;
	implications(*c1,*c2,std::back_inserter(impls));
	for (nf::const_iterator impl=impls.begin();
	     impl!=impls.end();++impl) {
	  for (nf::iterator c=f.begin();c!=f.end();)
	    if (c!=c1 && c!=c2 && subset_eq(*impl,*c))
	      c=f.erase(c);
	    else
	      ++c;
	  if (subset(*impl,*c1))
	    *c1=*impl;
	  if (subset(*impl,*c2))
	    *c2=*impl;
	}
      }
    }

    clause intersect;
    if (f.size()>1 && f.front().size()>1) {
      intersect=f.front();
      for (nf::const_iterator c=++f.begin();c!=f.end() && !intersect.empty();
	   ++c) {
	if (c->size()==1) {
	  intersect.clear();
	  break;
	}
	clause tmp;
	std::set_intersection(intersect.begin(),intersect.end(),
			      c->begin(),c->end(),inserter(tmp,tmp.begin()),
			      intersect.key_comp());
	intersect=tmp;
      }
    }

    if (sz!=number_of_literals(f) || !intersect.empty()) {
      for (nf::iterator c=f.begin();c!=f.end();++c) {
	for (clause::const_iterator it=intersect.begin();
	     it!=intersect.end();++it)
	  c->erase(*it);

	clause tmp;
	std::transform(c->begin(),c->end(),std::inserter(tmp,tmp.begin()),
		       std::negate<int>());
	*c=tmp;
      }

      clause tmp;
      std::transform(intersect.begin(),intersect.end(),
		     std::inserter(tmp,tmp.begin()),std::negate<int>());
      intersect=tmp;

      mapper.extract_cnf(f.begin(),f.end(),tr,it);

    }

    if (!intersect.empty()) {
      *it=id::logical_or;
      if (!it.is_childless()) {
	tr.prepend_child(it,id::logical_and);
	tr.reparent(it.begin(),++it.begin(),it.end());
      }
      mapper.extract_conjunction(intersect,tr,tr.append_child(it));
      if (intersect.size()>1)
	tr.erase(tr.flatten(it.last_child()));
    }
  }

  void subtree_to_enf::reduce_to_enf::operator()(sib_it it) {
    if (*it==id::logical_true || *it==id::logical_false)
      return;

    tr.sort_on_subtrees(it.begin(),it.end(),comp,true);
    for (up_it p=++tr.begin_upwards(it);
	 (p!=tr.end_upwards() && 
	  vertex_input_type(*p)==id::boolean &&
	  vertex_output_type(*p)==id::boolean);++p) {
      if (!util::is_sorted(make_counting_iterator(p.begin()),
			   make_counting_iterator(p.end()),comp))
	tr.sort_on_subtrees(p.begin(),p.end(),comp);
    }
    subtree_set dominant,command;
    build_subtree_sets_upwards(it,
			       inserter(dominant,dominant.end()),
			       inserter(command,command.end()));

    switch(reduce(it,dominant,command)) {
    case Delete: //a contradiction
      tr.erase_children(it);
      *it=id::logical_false;
      break;
    case Disconnect: //a tautology
      tr.erase_children(it);
      *it=id::logical_true;
      break;
    case Keep: //neither
      break;
    }
  }
    
  bool subtree_to_enf::reduce_to_enf::consistent(const subtree_set& s) {
    return 
      std::adjacent_find(make_indirect_iterator(s.begin()),
			 make_indirect_iterator(s.end()),
			 is_complement)==make_indirect_iterator(s.end());
  }

  bool subtree_to_enf::reduce_to_enf::and_cut(sib_it child) {
    bool adopted=false;
    for (sib_it gchild=child.begin();gchild!=child.end();)
      if (gchild.has_one_child()) {
	if (*gchild.begin()==id::logical_or) {
	  tr.erase(tr.flatten(gchild.begin()));
	  if (!adopted) //is child adopting a terminal 1-constrant AND, x?
	    for (sib_it x=gchild.begin();x!=gchild.end();++x)
	      if (x.has_one_child()) {
		adopted=true;
		break;
	      }
	  gchild=tr.erase(tr.flatten(gchild));
	} else {
	  assert(is_argument(*gchild.begin()));
	  gchild=tr.erase(tr.flatten(gchild));
	}
      } else {
	++gchild;
      }
    return adopted;
  }

  void subtree_to_enf::reduce_to_enf::or_cut(sib_it current) {
    for (sib_it child=current.begin();child!=current.end();)
      if (child.has_one_child() && (*child==id::logical_and ||
				    *child==id::logical_or)) {
	if (*child.begin()==id::logical_or) {
	  util::insert_set_complement
	    (tree_inserter(tr,current),
	     make_counting_iterator(current.begin()),
	     make_counting_iterator(current.end()),
	     make_counting_iterator(child.begin().begin()),
	     make_counting_iterator(child.begin().end()),comp);
	  tr.erase(tr.flatten(child.begin()));
	  child=tr.erase(tr.flatten(child));
	} else {
	  if (is_argument(*child.begin()))
	    child=tr.erase(tr.flatten(child));
	  else
	    child=tr.erase(child);
	}
      } else {
	++child;
      }
  }
      
  subtree_to_enf::reduce_to_enf::Result 
  subtree_to_enf::reduce_to_enf::reduce(sib_it current,
					const subtree_set& dominant,
					const subtree_set& command) {
    assert(util::is_sorted(dominant.begin(),dominant.end(),comp));

    //first remove duplicate children
    if (!current.is_childless()) {
      for (sib_it sib1=current.begin(),sib2=++current.begin();
	   sib2!=current.end();) {
	if (tr.equal_subtree(sib1,sib2)) {
	  sib2=tr.erase(sib2);
	} else {
	  sib1=sib2;
	  ++sib2;
	}
      }
    }

    assert(util::is_sorted(dominant.begin(),dominant.end(),comp));
    assert(util::is_sorted(command.begin(),command.end(),comp));

    if (*current==id::logical_and)
      return reduce_and(current,dominant,command);
    if (*current==id::logical_or)
      return reduce_or(current,dominant,command);

    //should be an argument or something
    //assert(is_argument(*current));
    return Keep;
  }

  subtree_to_enf::reduce_to_enf::Result 
  subtree_to_enf::reduce_to_enf::reduce_and(sib_it current,
					    const subtree_set& dominant,
					    const subtree_set& command) {
    util::erase_set_intersection(tree_eraser(tr),
				 make_counting_iterator(current.begin()),
				 make_counting_iterator(current.end()),
				 dominant.begin(),dominant.end(),comp);


    std::vector<vtree> negated;
    push_back_negated_arguments(command,negated);
    std::sort(negated.begin(),negated.end(),comp);

    util::erase_set_intersection(tree_eraser(tr),
				 make_counting_iterator(current.begin()),
				 make_counting_iterator(current.end()),
				 negated.begin(),negated.end(),comp);
				
    if (current.is_childless())
      return Disconnect; //0subsume
	
    if (!util::has_empty_intersection
	(make_counting_iterator(current.begin()),
	 make_counting_iterator(current.end()),
	 command.begin(),command.end(),comp))
      return Delete;     //1subsume
	  
    std::list<sib_it> prev_guard_set;
    do {
      prev_guard_set=
	std::list<sib_it>(make_counting_iterator(current.begin()),
			  make_counting_iterator(current.end()));
	    	  
      assert(util::is_sorted(dominant.begin(),dominant.end(),comp));
      if (!util::is_sorted(make_counting_iterator(current.begin()),
			   make_counting_iterator(current.end()),comp))
	tr.sort_on_subtrees(current.begin(),current.end(),comp);
	  
      subtree_set handle_set; //broom handle
      std::set_union(dominant.begin(),dominant.end(),
		     make_counting_iterator(current.begin()),
		     make_counting_iterator(current.end()),
		     inserter(handle_set,handle_set.begin()),comp);

      if (!consistent(handle_set))
	return Delete;

      for (sib_it child=current.begin();child!=current.end();) {
	if (child.is_childless() || (*child!=id::logical_and && 
				     *child!=id::logical_or)) {
	  ++child;
	  continue;
	}

	tr.validate(child);
	tr.validate();

	assert(util::is_sorted(command.begin(),command.end(),comp));
	assert(util::is_sorted(handle_set.begin(),handle_set.end(),comp));

	subtree_set::iterator tmp_it=handle_set.find(child);
	sib_it tmp;
	bool addIt=true;
	if (tmp_it!=handle_set.end()) {
	  tmp=*tmp_it;
	  handle_set.erase(tmp_it);
	} else {
	  addIt=false;
	}
	    
	switch(reduce(child,handle_set,command)) {
	case Delete:
	  return Delete; //since current is in all selections with child
	case Disconnect:
	  child=tr.erase(child);
	  addIt=false;
	  break;
	case Keep:

	  if (!util::is_sorted(make_counting_iterator(current.begin()),
			       make_counting_iterator(current.end()),
			       comp))
	    tr.sort_on_subtrees(current.begin(),current.end(),comp);

	  assert(child.begin()!=child.end()); //not sure if this is ok..
	  //make res the disjunction of child's children's guard sets
	  //important: we use references to the last child because we are
	  //later going to iterate (left-to-right) removing these iterators
	  //from the tree - unless we have references *only* to the
	  //last-to-be-removed items, res may end up containing iterators
	  //pointing at tree nodes that already have been removed
	  subtree_set res(make_counting_iterator(child.last_child().begin()),
			  make_counting_iterator(child.last_child().end()));
	  for (sib_it sib=child.begin();sib!=child.last_child();++sib)
	    util::erase_set_difference(subtree_set_eraser(res),
				       res.begin(),res.end(),
				       make_counting_iterator(sib.begin()),
				       make_counting_iterator(sib.end()),
				       comp);

	  if (!res.empty()) {
	    util::insert_set_complement
	      (tree_inserter(tr,current),
	       make_counting_iterator(current.begin()),
	       make_counting_iterator(current.end()),
	       res.begin(),res.end(),comp);
		
	    for (sib_it gchild=child.begin();gchild!=child.end();++gchild)
	      util::erase_set_intersection
		(tree_eraser(tr),
		 make_counting_iterator(gchild.begin()),
		 make_counting_iterator(gchild.end()),
		 res.begin(),res.end(),comp);

	    //try to apply and-cut to child's children
	    and_cut(child);

	    //guard_set has been enlarged, so we need to reprocess all kids
	    child=current.begin();
	  } else {
	    if (!and_cut(child)) //if child is adopting a terminal 
	      ++child;           //1-constrant AND, need to reprocess it
	  }
	}
	if (addIt)
	  handle_set.insert(tmp);
      }

      //try to apply or-cut to current's children
      or_cut(current);
    } while (current.number_of_children()!=prev_guard_set.size() ||
	     !std::equal(prev_guard_set.begin(),prev_guard_set.end(),
			 make_counting_iterator(current.begin())));
	
    if (current.is_childless())
      return Disconnect; //0-subsumption
    return Keep;
  }

  subtree_to_enf::reduce_to_enf::Result 
  subtree_to_enf::reduce_to_enf::reduce_or(sib_it current,
					   const subtree_set& dominant,
					   const subtree_set& command) {

    for (sib_it child=current.begin();child!=current.end();) {
      subtree_set child_command(command);
      for (sib_it sib=current.begin();sib!=current.end();++sib)
	if (sib!=child && sib.has_one_child())
	  child_command.insert(sib.begin());
      switch(reduce(child,dominant,child_command)) {
      case Delete:
	if (current.is_childless() || current.has_one_child())
	  return Delete;
	else 	    
	  child=tr.erase(child);
	break;
      case Disconnect:
	return Disconnect;
      case Keep:
	++child;
      }
    }
    return Keep;
  }

} //~namespace reduct

