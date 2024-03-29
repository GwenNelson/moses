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

#include "eda/field_set.h"
#include <sstream>
#include "util/dorepeat.h"

namespace eda {
  const disc_t field_set::contin_spec::Stop=0;
  const disc_t field_set::contin_spec::Left=1;
  const disc_t field_set::contin_spec::Right=2;

  const disc_t field_set::onto_spec::Stop=0;

  field_set& field_set::operator=(const field_set& rhs) {
    _fields=rhs._fields;
    _disc=rhs._disc;
    _contin=rhs._contin;
    _onto=rhs._onto;
    _nbool=rhs._nbool;
    _contin_start=_fields.begin()+distance(rhs._fields.begin(),rhs._contin_start);
    _disc_start=_fields.begin()+distance(rhs._fields.begin(),rhs._disc_start);
    return *this;
  }
  bool field_set::operator==(const field_set& rhs) const {
    return (_disc==rhs._disc && _contin==rhs._contin &&
	    _onto==rhs._onto && _nbool==rhs._nbool);
  }

  const onto_t& field_set::get_onto(const instance& inst,size_t idx) const {
    size_t raw_idx=onto_to_raw_idx(idx);
    //walk down the tree to get the appropriate onto_t
    const onto_spec& o=_onto[idx];
    onto_tree::iterator it=o.tr->begin();
    for (arity_t i=0;i<o.depth;++i) {
      disc_t raw_value=get_raw(inst,raw_idx+i);
      if (raw_value==onto_spec::Stop) {
	break;
      } else {
	it=o.tr->child(it,onto_spec::to_child_idx(raw_value));
      }
    }
    return *it;
  }

  contin_t field_set::get_contin(const instance& inst,size_t idx) const {
    size_t raw_idx=contin_to_raw_idx(idx);
    //start with the mean a walk down to the res
    const contin_spec& c=_contin[idx];
    contin_stepper stepper(c);
    for (arity_t i=0;i<c.depth;++i) {
      disc_t direction=get_raw(inst,raw_idx+i); 
      if (direction==contin_spec::Stop) {
	break;
      } else if (direction==contin_spec::Left) {
	stepper.left();
      } else { //direction==contin_spec::Right
	assert(direction==contin_spec::Right);
	stepper.right();
      }
    }
    return stepper.value;
  }

  void field_set::set_contin(instance& inst,size_t idx,contin_t target) const {
    size_t raw_idx=contin_to_raw_idx(idx);
    const contin_spec& c=_contin[idx];

    //use binary search to assign to the nearest value
    contin_t best_distance=fabs(c.mean-target);
    arity_t best_depth=0;
    contin_stepper stepper(c);
    for (arity_t i=0;i<c.depth && best_distance>c.epsilon();++i) {
      //take a step in the correct direction
      if (target<stepper.value) { //go left
	stepper.left();
	set_raw(inst,raw_idx+i,contin_spec::Left);
      } else { //target>stepper.value - go right
	stepper.right();
	set_raw(inst,raw_idx+i,contin_spec::Right);
      }
      if (fabs(stepper.value-target)<best_distance) {
	best_distance=fabs(stepper.value-target);
	best_depth=i+1;
      }
    }

    //backtrack up to the best depth
    for (arity_t i=best_depth;i<c.depth;++i)
      set_raw(inst,raw_idx+i,contin_spec::Stop);      
  }

  //note to self: changed this on 4/23 - its confusing that stream and
  //stream_raw give different orderings among the various types of vars - what
  //was I thinking?
  std::string field_set::stream(const instance& inst) const { 
    std::stringstream ss;
    ss << "[";

    if (begin_onto(inst)!=end_onto(inst))
      ss << "#";
    copy(begin_onto(inst),end_onto(inst),
	 std::ostream_iterator<onto_t>(ss,"#"));

    if (begin_contin(inst)!=end_contin(inst))
      ss << "|";
    copy(begin_contin(inst),end_contin(inst),
	 std::ostream_iterator<contin_t>(ss,"|"));

    if (begin_disc(inst)!=end_disc(inst))
      ss << " ";
    copy(begin_disc(inst),end_disc(inst),std::ostream_iterator<disc_t>(ss," "));

    copy(begin_bits(inst),end_bits(inst),std::ostream_iterator<bool>(ss,""));

    ss << "]";
    return ss.str();
  }

  std::string field_set::stream_raw(const instance& inst) const { 
    std::stringstream ss;
    ss << "[";
    copy(begin_raw(inst),end_raw(inst),std::ostream_iterator<disc_t>(ss,""));
    ss << "]";
    return ss.str();
  }

  void field_set::build_spec(const spec& s,size_t n) {
    if (const disc_spec* d=boost::get<disc_spec>(&s)) {
      arity_t width=util::nbits_to_pack(d->arity);
      size_t base=back_offset();
      for (size_t idx=0;idx<n;++idx)
	_fields.push_back(field(width,(base+idx*width)/bits_per_packed_t,
				(base+idx*width)%bits_per_packed_t));
      _disc.insert(_disc.end(),n,*d);
      if (width==1)
	_nbool+=n;
    } else if (const contin_spec* c=boost::get<contin_spec>(&s)) {
      assert(c->depth==util::next_power_of_two(c->depth)); //must be 2^n
      //all have arity of 3 (left, right, or stop) and hence are 2 wide
      size_t base=back_offset(),width=2;
      dorepeat(n*c->depth) {
	_fields.push_back(field(width,base/bits_per_packed_t,
				base%bits_per_packed_t));
	base+=width;
      }
      _contin.insert(_contin.end(),n,*c);
    } else {
      const onto_spec* o=boost::get<onto_spec>(&s);
      assert(o);
      size_t base=back_offset(),width=util::nbits_to_pack(o->branching);
      size_t total_width=size_t((width*o->depth-1)/
				bits_per_packed_t+1)*bits_per_packed_t;

      dorepeat(n) {
	dorepeat(o->depth) {
	  _fields.push_back(field(width,base/bits_per_packed_t,
				  base%bits_per_packed_t));
	  base+=width;
	}
	base+=total_width-(o->depth*width); //onto vars must pack evenly
      }
      _onto.insert(_onto.end(),n,*o);
    }
  }

} //~namespace eda
