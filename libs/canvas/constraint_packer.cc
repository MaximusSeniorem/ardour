/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>

#include "pbd/i18n.h"
#include "pbd/unwind.h"
#include "pbd/stacktrace.h"

#include "kiwi/kiwi.h"

#include "canvas/constraint_packer.h"
#include "canvas/constrained_item.h"
#include "canvas/item.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;

using std::cerr;
using std::endl;
using std::vector;
using kiwi::Constraint;
using namespace kiwi;

ConstraintPacker::ConstraintPacker (Canvas* canvas)
	: Container (canvas)
	, width (X_("packer width"))
	, height (X_("packer height"))
	, in_alloc (false)
	, _need_constraint_update (false)
{
	set_fill (false);
	set_outline (false);
	set_layout_sensitive (true);

	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);
}

ConstraintPacker::ConstraintPacker (Item* parent)
	: Container (parent)
	, width (X_("packer width"))
	, height (X_("packer height"))
	, in_alloc (false)
	, _need_constraint_update (false)
{
	set_fill (false);
	set_outline (false);
	set_layout_sensitive (true);

	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);
}

void
ConstraintPacker::compute_bounding_box () const
{
	_bounding_box = _allocation;
	_bounding_box_dirty = false;
}

void
ConstraintPacker::child_changed (bool bbox_changed)
{
	Item::child_changed (bbox_changed);

	if (in_alloc || !bbox_changed) {
		return;
	}
#if 0
	cerr << "CP, child bbox changed\n";

	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

		Duple i = x->first->intrinsic_size();

		if (r) {

			// cerr << x->first->whatami() << '/' << x->first->name << " has instrinsic size " << r << endl;

			kiwi::Variable& w (x->second->intrinsic_width());
			if (!r.width()) {
				if (_solver.hasEditVariable (w)) {
					_solver.removeEditVariable (w);
					cerr << "\tremoved inttrinsic-width edit var\n";
				}
			} else {
				if (!_solver.hasEditVariable (w))  {
					cerr << "\tadding intrinsic width constraints\n";
					_solver.addEditVariable (w, kiwi::strength::strong);
					_solver.addConstraint (Constraint {x->second->width() >= w } | kiwi::strength::strong);
					_solver.addConstraint (Constraint (x->second->width() <= w) | kiwi::strength::weak);
				}
			}

			kiwi::Variable& h (x->second->intrinsic_height());
			if (!r.height()) {
				if (_solver.hasEditVariable (h)) {
					_solver.removeEditVariable (h);
					cerr << "\tremoved inttrinsic-height edit var\n";
				}
			} else {
				if (!_solver.hasEditVariable (h))  {
					cerr << "\tadding intrinsic height constraints\n";
					_solver.addEditVariable (h, kiwi::strength::strong);
					_solver.addConstraint (Constraint {x->second->height() >= h } | kiwi::strength::strong);
					_solver.addConstraint (Constraint (x->second->height() <= h) | kiwi::strength::weak);
				}
			}
		}
	}
#endif
}

void
ConstraintPacker::constrain (kiwi::Constraint const &c)
{
	constraint_list.push_back (c);
	_need_constraint_update = true;
}

void
ConstraintPacker::preferred_size (Duple& minimum, Duple& natural) const
{
#if 0
	/* our parent wants to know how big we are.

	   We may have some intrinsic size (i.e. "everything in this constraint
	   layout should fit into WxH". Just up two constraints on our width
	   and height, and solve.

	   We may have one intrinsic dimension (i.e. "everything in this
	   constraint layout should fit into this (width|height). Ask all of
	   our children for the size-given-(W|H). Add constraints to represent
	   those values, and solve.

	   We may have no intrinsic dimensions at all. This is the tricky one.
	*/

	if (_need_constraint_update) {
		const_cast<ConstraintPacker*>(this)->update_constraints ();
	}

	if (_intrinsic_width > 0) {
		_solver.suggestValue (width, _intrinsic_width);
	} else if (_intrinsic_height > 0) {
		_solver.suggestValue (height, _intrinsic_height);
	}

	_solver.updateVariables ();

	Duple ret;

	natural.x = width.value ();
	natural.y = height.value ();

	minimum = natural;
#endif
	natural.x = 100;
	natural.y = 100;
	minimum = natural;

	cerr << "CP::sr returns " << natural<< endl;
}

void
ConstraintPacker::size_allocate (Rect const & r)
{
	PBD::Unwinder<bool> uw (in_alloc, true);

	Item::size_allocate (r);

	if (_need_constraint_update) {
		update_constraints ();
	}

	_solver.suggestValue (width, r.width());
	_solver.suggestValue (height, r.height());
	_solver.updateVariables ();

#if 0
	_solver.dump (cerr);

	for (ConstrainedItemMap::const_iterator o = constrained_map.begin(); o != constrained_map.end(); ++o) {
		o->second->dump (cerr);
	}
#endif

	apply (0);

	_bounding_box_dirty = true;
}

void
ConstraintPacker::add (Item* item)
{
	(void) add_constrained (item);
}

void
ConstraintPacker::add_front (Item* item)
{
	(void) add_constrained (item);
}

void
ConstraintPacker::add_constraints (Solver& s, ConstrainedItem* ci) const
{
	/* add any constraints inherent to this item */

	vector<Constraint> const & vc (ci->constraints());

	for (vector<Constraint>::const_iterator x = vc.begin(); x != vc.end(); ++x) {
		s.addConstraint (*x);
	}
}

ConstrainedItem*
ConstraintPacker::add_constrained (Item* item)
{
	ConstrainedItem* ci =  new ConstrainedItem (*item);
	add_constrained_internal (item, ci);
	return ci;
}

void
ConstraintPacker::add_constrained_internal (Item* item, ConstrainedItem* ci)
{
	Item::add (item);
	item->set_layout_sensitive (true);
	constrained_map.insert (std::make_pair (item, ci));
	_need_constraint_update = true;
	child_changed (true);
}

void
ConstraintPacker::remove (Item* item)
{
	Item::remove (item);

	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

		if (x->first == item) {

			/* remove any non-builtin constraints for this item */

			for (ConstraintList::iterator c = constraint_list.begin(); c != constraint_list.end(); ++c) {
				if (x->second->involved (*c)) {
					constraint_list.erase (c);
				}
			}

			item->set_layout_sensitive (false);

			/* clean up */

			delete x->second;
			constrained_map.erase (x);
			break;
		}

	}

	_need_constraint_update = true;
}

void
ConstraintPacker::apply (Solver* s)
{
	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {
		x->second->constrained (*this);
	}
}

void
ConstraintPacker::update_constraints ()
{
	_solver.reset ();
	_solver.addEditVariable (width, kiwi::strength::strong);
	_solver.addEditVariable (height, kiwi::strength::strong);

	for (ConstrainedItemMap::iterator x = constrained_map.begin(); x != constrained_map.end(); ++x) {

		Duple min, natural;
		ConstrainedItem* ci = x->second;

		x->first->preferred_size (min, natural);

		_solver.addConstraint (ci->width() >= min.width() | kiwi::strength::required);
		_solver.addConstraint (ci->height() >= min.height() | kiwi::strength::required);
		_solver.addConstraint (ci->width() == natural.width() | kiwi::strength::medium);
		_solver.addConstraint (ci->height() == natural.width() | kiwi::strength::medium);

		add_constraints (_solver, ci);
	}

	for (ConstraintList::const_iterator c = constraint_list.begin(); c != constraint_list.end(); ++c) {
		_solver.addConstraint (*c);
	}
}
