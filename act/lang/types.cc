/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2017-2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include <stdio.h>
#include <act/act.h>
#include <act/types.h>
#include <act/iter.h>
#include <string.h>
#include <common/misc.h>
#include <common/hash.h>

const char *act_builtin_method_name[ACT_NUM_STD_METHODS] =
  { "set", "get", "send_rest", "recv_rest", "send_up", "recv_up",
    "send_init", "recv_init" };

const char *act_builtin_method_expr[ACT_NUM_EXPR_METHODS] =
  { "send_probe", "recv_probe" };

const int act_builtin_method_boolret[ACT_NUM_EXPR_METHODS] =  { 1, 1 };

/*------------------------------------------------------------------------
 *
 *  Parameter types (preal, pbool, pint, ptype)
 *
 *------------------------------------------------------------------------
 */
void Type::Init ()
{
  TypeFactory::Init();
}

const char *UserDef::getName()
{
  return name;
}

void UserDef::printActName (FILE *fp)
{
  int i = 0;
  int in_param = 0;

  while (name[i]) {
    if (!in_param && name[i] != '<') {
      fputc (name[i], fp);
    }
    else if (in_param) {
      if (name[i] == 't' && name[i-1] == ',') {
	fprintf (fp, "true");
      }
      else if (name[i] == 'f' && name[i-1] == ',') {
	fprintf (fp, "false");
      }
      else {
	fputc (name[i], fp);
      }
    }
    else {
      /* name[i] == '<' */
      if (name[i+1] != '>') {
	fputc (name[i], fp);
	in_param = 1;
      }
      else {
	return;
      }
    }
    i++;
  }
}

int UserDef::AddMetaParam (InstType *t, const char *id)
{
  int i;

  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return 0;
    }
  }

  /*printf ("ADD: %s to userdef: %x\n", id, this);*/

  if (!I->Add (id, t)) {
    return 0;
  }
  if (expanded) {
    ValueIdx *vx = I->LookupVal (id);
    Assert (vx, "What?");
    /* port parameters are immutable */
    vx->immutable = 1;
  }

  nt++;
  
  if (pn) {
    REALLOC (pn, const char *, nt);
  }
  else {
    NEW (pn, const char *);
  }
  pn[nt-1] = id;

  if (pt) {
    REALLOC (pt, InstType *, nt);
  }
  else {
    NEW (pt, InstType *);
  }
  pt[nt-1] = t;

  return 1;
}



const char *UserDef::getPortName (int pos) const
{
  if (pos < 0) {
    pos = -pos;
    Assert (1 <= pos && pos < 1+getNumParams(), "Invalid pos!");
    return pn[pos-1];
  }
  else {
    Assert (0 <= pos && pos < getNumPorts(), "Invalid pos!");
    return port_n[pos];
  }
}

InstType *UserDef::getPortType (int pos) const
{
  if (pos < 0) {
    pos = -pos;
    Assert (1 <= pos && pos < 1+getNumParams(), "Invalid pos!");
    return pt[pos-1];
  }
  else {
    Assert (0 <= pos && pos < getNumPorts(), "Invalid pos!");
    return port_t[pos];
  }
}

void UserDef::refinePortType (int pos, InstType *u)
{
  Assert (pos >= 0, "Can't refine parameter types");
  Assert (pos < getNumPorts(), "Invalid pos!");
  port_t[pos] = port_t[pos]->refineBaseType (u);
}

int UserDef::AddPort (InstType *t, const char *id)
{
  int i;
  
  /*printf ("ADDport: %s to scope %x\n", id, I);*/

  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return 0;
    }
  }
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return 0;
    }
  }

  if (!I->Add (id, t)) {
    return 0;
  }

  nports++;

  if (!port_n) {
    NEW (port_n, const  char *);
  }
  else {
    REALLOC (port_n, const char *, nports);
  }
  if (!port_t) {
    NEW (port_t, InstType *);
  }
  else {
    REALLOC (port_t, InstType *, nports);
  }

  port_n[nports-1] = id;
  port_t[nports-1] = t;

  return 1;
}

int UserDef::FindPort (const char *id)
{
  int i;
  
  for (i=0; i < nt; i++) {
    if (strcmp (pn[i], id) == 0) {
      return -(i+1);
    }
  }
  for (i=0; i < nports; i++) {
    if (strcmp (port_n[i], id) == 0) {
      return (i+1);
    }
  }
  return 0;
}


UserDef::UserDef (ActNamespace *ns)
{
  level = ACT_MODEL_PRS;	/* default */
  parent = NULL;

  defined = 0;
  expanded = 0;
  pending = 0;

  nt = 0;
  pt = NULL;
  pn = NULL;
  exported = 0;

  name = NULL;
  
  nports = 0;
  port_t = NULL;
  port_n = NULL;
  unexpanded = NULL;

  b = NULL;

  lang = new act_languages ();

  I = new Scope(ns->CurScope());
  I->setUserDef (this);
  _ns = ns;

  file = NULL;
  lineno = 0;

  has_refinement = 0;

  inherited_templ = 0;
  inherited_param = NULL;

  A_INIT (um);
}

UserDef::~UserDef()
{
  int i;
  if (pt) {
    for (i=0; i < nt; i++) {
      if (pt[i]) {
	delete pt[i];
      }
    }
    FREE (pt);
  }
  if (pn) {
    FREE (pn);
  }
  if (port_t) {
    for (i=0; i < nports; i++) {
      if (port_t[i]) {
	delete port_t[i];
      }
    }
    FREE (port_t);
  }
  if (port_n) {
    FREE (port_n);
  }
  if (I) {
    delete I;
  }
  if (b) {
    delete b;
  }

  if (A_LEN (um) > 0) {
    for (int i=0; i < A_LEN (um); i++) {
      Assert (um[i], "NULL macro?");
      delete um[i];
    }
    A_FREE (um);
  }

  if (inherited_param) {
    FREE (inherited_param);
  }

  if (lang) {
    delete lang;
  }
}

void UserDef::MkCopy (UserDef *u)
{
  parent = u->parent; u->parent = NULL;
  defined  = u->defined; u->defined = 0;
  expanded = u->expanded; u->expanded = 0;
  pending = u->pending; u->pending = 0;

  lang = u->lang;
  u->lang = new act_languages ();

  nt = u->nt; u->nt = 0;
  
  pt = u->pt; u->pt = NULL;
  pn = u->pn; u->pn = NULL;

  exported = u->exported; u->exported = 0;

  nports = u->nports; u->nports = 0;
  port_t = u->port_t; u->port_t = NULL;
  port_n = u->port_n; u->port_n = NULL;

  I = u->I; u->I = NULL;
  I->setUserDef (this);

  name = u->name; u->name = NULL;
  b = u->b; u->b = NULL;
  _ns = u->_ns; u->_ns = NULL;

  unexpanded = u->unexpanded;
  level = u->level;

  file = u->file;
  lineno = u->lineno;
  has_refinement = u->has_refinement;

  inherited_templ = u->inherited_templ;
  inherited_param = u->inherited_param;
  u->inherited_param = NULL;
  u->inherited_templ = 0;

  A_ASSIGN (um, u->um);
  u->um = NULL;
}


Function::Function (UserDef *u) : UserDef (u)
{
  b = NULL;
  ret_type = NULL;
  is_simple_inline = 0;
}

Function::~Function ()
{
  if (b) {
    delete b;
  }
}


Interface::Interface (UserDef *u) : UserDef (u)
{

}

Interface::~Interface() { }


void UserDef::SetParent (InstType *t)
{
  UserDef *x;
  parent = t;

  if (!t) return;

  x = dynamic_cast<UserDef *>(t->BaseType());
  if (x && x->getNumParams() > 0) {
    if (x->inherited_templ > 0 || t->getNumParams() > 0) {
      int i = 0;
      inst_param *ip = NULL;
      MALLOC (inherited_param, inst_param *, nt);

      for (int j=0; j < nt; j++) {
	inherited_param[j] = NULL;
      }

      if (x->inherited_templ > 0) {
	i = nt - x->getNumParams();
	for (int j=0; j < x->inherited_templ; j++) {
	  inherited_param[i+j] = x->inherited_param[j];
	  inherited_templ++;
	}
      }
      if (t->getNumParams() > 0) {
	ip = t->allParams ();
	i = nt - x->getNumParams();
	for (int j=0; j < t->getNumParams(); j++) {
	  while (inherited_param[i] && i < nt) {
	    i++;
	  }
	  if (i == nt) {
	    fatal_error ("Too many template parameters specified?");
	  }
	  inherited_param[i++] = &ip[j];
	  inherited_templ++;
	}
      }
    }
  }
}


int UserDef::isStrictPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < nt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int UserDef::isPort (const  char *s)
{
  int i;

  /*printf ("mt = %d, nt = %d [%x]\n", mt, nt, this);*/

  for (i=0; i < nports; i++) {
    if (strcmp (s, port_n[i]) == 0) {
      return 1;
    }
  }
  for (i=0; i < nt; i++) {
    if (strcmp (s, pn[i]) == 0) {
      return 1;
    }
  }
  return 0;
}


Data::Data (UserDef *u) : UserDef (u)
{
  int i;

  is_enum = 0;
  b = NULL;

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = NULL;
  }
}

Data::~Data()
{
  
}

Channel::Channel (UserDef *u) : UserDef (u)
{
  int i;

  b = NULL;

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = NULL;
  }
  for (i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    emethods[i] = NULL;
  }
}

Channel::~Channel()
{
  
}


/*------------------------------------------------------------------------
 *  Compare two user-defined types
 *------------------------------------------------------------------------*/
int UserDef::isEqual (const UserDef *u) const
{
  int i;

  if (parent != u->parent) {
    if (!parent || !u->parent) return 0;
    if (!parent->isEqual (u->parent)) {
      return 0;
    }
  }
  if (nt != u->nt) return 0;
  if (nports != u->nports) return 0;

  if (exported != u->exported) return 0;
  if (expanded != u->expanded) return 0;

  for (i=0; i < nt; i++) {
    if (pn[i] != u->pn[i]) return 0;
    if (!pt[i]->isEqual (u->pt[i])) return 0;
  }

  for (i=0; i < nports; i++) {
    if (port_n[i] != u->port_n[i]) return 0;
    if (!port_t[i]->isEqualDir (u->port_t[i])) return 0;
  }
  
  return 1;
}



  

static int recursion_depth = 0;

/*------------------------------------------------------------------------
 *
 *   Expand user-defined type! 
 *
 *------------------------------------------------------------------------
 */
UserDef *UserDef::Expand (ActNamespace *ns, Scope *s,
			  int spec_nt, inst_param *u,
			  int *cache_hit, int is_proc)
{
  UserDef *ux;
  int k, sz, len;
  InstType *x, *p;  
  Array *xa;

  recursion_depth++;

  if (recursion_depth >= Act::max_recurse_depth) {
    act_error_ctxt (stderr);
    fatal_error ("Exceeded maximum recursion depth of %d\n", Act::max_recurse_depth);
  }

  /* nt = # of specified parameters
     u = expanded instance paramters
  */
#if 0
  fprintf (stderr, "[In expand userdef] %s [parent: %d]\n", getName(),
	   inherited_templ);
  fprintf (stderr, "  nt=%d, spec_nt=%d  <", nt, spec_nt);
  for (int i=0; i < spec_nt; i++) {
    u[i].u.tp->Print (stderr);
    if (i != spec_nt-1) {
      fprintf (stderr, " , ");
    }
  }
  fprintf (stderr, ">\n");
#endif

  /* create a new userdef type */
  ux = new UserDef (_ns);
  ux->unexpanded = this;
  ux->file = file;
  ux->lineno = lineno;
  ux->has_refinement = has_refinement;

  if (defined) {
    ux->MkDefined();
  }

  /* set its scope to "expanded" mode */
  ux->I->FlushExpand();
  if (is_proc == 2) {
    ux->I->mkFunction();
  }
  /* set to pending */
  ux->pending = 1;
  ux->expanded = 1;

  InstType *instparent;
  UserDef *uparent;

  instparent = parent;
  if (instparent) {
    uparent = dynamic_cast <UserDef *> (instparent->BaseType());
  }
  else {
    uparent = NULL;
  }

  /* create bindings for type parameters */
  int i;
  int ii = 0;

  for (i=0; i < nt; i++) {
    p =  getPortType (-(i+1));
    if (!p) {
      Assert (0, "Enumeration?");
      /* this is an enumeration type, there is nothing to be done here */
      ux->AddMetaParam (NULL, pn[i]);
    }
    else {
      x = p->Expand (ns, ux->I); // this is the real type of the
				 // parameter

      if (x->arrayInfo() && x->arrayInfo()->size() == 0) {
	act_error_ctxt (stderr);
	fatal_error ("Template parameter `%s': zero-length array creation not permitted", getPortName (-(i+1)));
      }

      /* add parameter to the scope */
      ux->AddMetaParam (x, pn[i]);

      /* this one gets bound to:
	  - the next specified parameter, if it exists
	  - the next inherited parameter, if it exists 
      */
      inst_param *bind_param = NULL;
      int bind_src = 0;
      if (inherited_templ > 0 && inherited_param[i]) {
	bind_param = inherited_param[i];
	bind_src = 0;
      }
      else if (ii < spec_nt) {
	bind_param = &u[ii];
	bind_src = 1;
	ii++;
      }
      else {
	bind_param = NULL;
      }

      if (!bind_param) continue;

      if (TypeFactory::isPTypeType (p->BaseType())) {
	if (!bind_param->isatype) {
	  AExpr *rhsval = bind_param->u.tp;
	  x = rhsval->isType();
	}
	else {
	  x = bind_param->u.tt;
	}
	if (x) {
	  x = x->Expand (ns, ux->I);
	  ux->I->BindParam (pn[i], x);
	}
	else {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "   Parameter: ");
	  bind_param->u.tp->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Typechecking failed for parameter %d: expected type.", i);
	}
      }
      else  {
	InstType *rhstype;

	if (bind_param->isatype) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "   Parameter: ");
	  bind_param->u.tt->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Typechecking failed for parameter %d: expected value.", i);
	}

	AExpr *rhsval = bind_param->u.tp;
        if (!rhsval->isArrayExpr()) {
         rhsval = rhsval->Expand (ns, ux->I);
        }
	rhstype = rhsval->getInstType (ux->I, NULL, 1);
	if (type_connectivity_check (x, rhstype) != 1) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, "Typechecking failed, ");
	  x->Print (stderr);
	  fprintf (stderr, "  v/s ");
	  rhstype->Print (stderr);
	  fprintf (stderr, "\n\t%s\n", act_type_errmsg());
	  exit (1);
	}
#if 0	
	printf ("    <> bind %s := ", pn[i]);
	rhsval->Print (stdout);
	printf ("\n");
#endif	
	ux->I->BindParam (pn[i], rhsval);
	delete rhstype;
        if (!rhsval->isArrayExpr()) {
	   delete rhsval;
        }
      }
    }
  }

  int parent_start;
  if (parent && TypeFactory::isUserType (parent)) {
    parent_start = (nt - (dynamic_cast<UserDef *> (parent->BaseType()))->getNumParams());
    if (parent_start >= spec_nt) {
      parent_start = -1;
    }
  }
  else {
    parent_start = -1;
  }
  
  /*
     create a name for it!
     (old name)"<"string-from-types">"

     create a fresh scope

     expand out the body of the type

     insert the new type into the namespace into the xt table
  */

  sz = 0;

  /* type< , , , ... , > + end-of-string */
  /* for arrays, originally we had {a,b,c}... the expanded values.
     instead, we now have scope#idx
  */
  sz = strlen (getName()) + 3 + nt;

  for (int i=0; i < nt; i++) {
    InstType *x;
    Array *xa;
    ValueIdx *vx;

    x = ux->getPortType (-(i+1));
    vx = ux->I->LookupVal (pn[i]);
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (xa) {
	act_error_ctxt (stderr);
	fatal_error ("ptype array parameters not supported");
      }

      if (i < spec_nt) {
	if (u[i].isatype) {
	  x = u[i].u.tt;
	}
	else {
	  x = u[i].u.tp->isType();
	  if (!x) {
	    act_error_ctxt (stderr);
	    fprintf (stderr, "Typechecking failed for param #%d ", i);
	    u[i].u.tp->Print (stderr);
	    exit (1);
	  }
	}
      }
      else {
	x = NULL;
      }
      /* x is now the value of the parameter */
      if (x) {
	sz += strlen (x->BaseType()->getName()) + 3;
      }
      /* might have directions, upto 2 characters worth, @ for type */
    }
    else {
      /* check array info */
      if (vx->init) {
	if (xa) {
	  Assert (xa->isExpanded(), "Array info is not expanded");
	  sz += 32*xa->size()+2;
	}
	else {
	  sz += 32;
	}
      }
    }
  }

  char *buf;
  MALLOC (buf, char, sz);
  k = 0;
  buf[k] = '\0';
  snprintf (buf+k, sz, "%s<", getName());
  len = strlen (buf+k);
  k += len;
  sz -= len;

  ii=0;

  for (int i=0; i < nt; i++) {
    ValueIdx *vx;

    if (inherited_templ > 0 && inherited_param[i]) {
      continue;
    }
    if (ii == spec_nt) {
      break;
    }

    if (ii != 0) {
      snprintf (buf+k, sz, ",");
      len = strlen (buf+k); k += len; sz -= len;
    }
    Assert (sz > 0, "Check");
    x = ux->getPortType (-(i+1));
    vx = ux->I->LookupVal (pn[i]);
    xa = x->arrayInfo();
    if (TypeFactory::isPTypeType (x->BaseType())) {
      if (ii < spec_nt) {
	if (u[ii].isatype) {
	  x = u[ii].u.tt;
	}
	else {
	  x = u[ii].u.tp->isType();
	}	  
      }
      else {
	x = NULL;
      }
      if (x) {
	snprintf (buf+k, sz, "@%s%s", x->BaseType()->getName(),
		  Type::dirstring (x->getDir()));
	len = strlen (buf+k); k += len; sz -= len;
      }
    }
    else {
      if (vx->init) {
	if (xa) {
	  Arraystep *as;
	  
	  snprintf (buf+k, sz, "{");
	  k++; sz--;

	  as = xa->stepper();
	  while (!as->isend()) {
	    if (TypeFactory::isPIntType (x->BaseType())) {
	      snprintf (buf+k, sz, "%lu",
			ux->I->getPInt (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPIntsType (x->BaseType())) {
	      snprintf (buf+k, sz, "%ld",
			ux->I->getPInts (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPRealType (x->BaseType())) {
	      snprintf (buf+k, sz, "%g",
			ux->I->getPReal (vx->u.idx + as->index()));
	    }
	    else if (TypeFactory::isPBoolType (x->BaseType())) {
	      snprintf (buf+k, sz, "%c",
			ux->I->getPBool (vx->u.idx + as->index()) ? 't' : 'f');
	    }
	    else {
	      fatal_error ("What type is this?");
	    }
	    len = strlen (buf+k); k+= len; sz-= len;
	    as->step();
	    if (!as->isend()) {
	      snprintf (buf+k, sz, ",");
	      k++; sz--;
	    }
	  }
	  delete as;
	  snprintf (buf+k, sz, "}");
	  k++; sz--;
	}
	else {
	  if (TypeFactory::isPIntType (x->BaseType())) {
	    snprintf (buf+k, sz, "%lu", ux->I->getPInt (vx->u.idx));
	  }
	  else if (TypeFactory::isPIntsType (x->BaseType())) {
	    snprintf (buf+k, sz, "%ld", ux->I->getPInts (vx->u.idx));
	  }
	    else if (TypeFactory::isPRealType (x->BaseType())) {
	      snprintf (buf+k, sz, "%g", ux->I->getPReal (vx->u.idx));
	    }
	    else if (TypeFactory::isPBoolType (x->BaseType())) {
	      snprintf (buf+k, sz, "%c", ux->I->getPBool (vx->u.idx) ? 't' : 'f');
	    }
	    else {
	      fatal_error ("What type is this?");
	    }
	    len = strlen (buf+k); k+= len; sz-= len;
	}
      }
    }
    ii++;
  }
  snprintf (buf+k, sz, ">");
  k++; sz--;

  Assert (sz >= 0, "Hmmmmm");

  /* now we have the string for the type! */
  UserDef *uy;

  uy = _ns->findType (buf);

  if (uy) {
    if (uy->pending) {
      act_error_ctxt (stderr);
      fatal_error ("Recursive construction of type `%s'", buf);
    }
    FREE (buf);
    /* we found one! */
    delete ux;
    recursion_depth--;
    *cache_hit = 1;
    return uy;
  }
  *cache_hit = 0;

  Assert (_ns->CreateType (buf, ux), "Huh");
  FREE (buf);

  if (parent) {
    uparent = dynamic_cast <UserDef *> (parent->BaseType());
    if (uparent) {
      if (parent_start != -1) {
	InstType *parent2 = new InstType (parent);
	parent2->appendParams (spec_nt - parent_start, u+parent_start);
	ux->SetParent (parent2->Expand (ns, ux->CurScope()));
	delete parent2;
      }
      else {
	ux->SetParent (parent->Expand (ns, ux->CurScope()));
      }
    }
    else {
      ux->SetParent (parent->Expand (ns, ux->CurScope()));
    }
  }
  if (ux->parent) {
    ux->parent->MkCached ();
  }
  ux->exported = exported;

  /*-- create ports --*/
  for (int i=0; i < nports; i++) {
    InstType *chk;
    Assert (ux->AddPort ((chk = getPortType(i)->Expand (ns, ux->I)),
			 getPortName (i)), "What?");

    if (chk->arrayInfo() && chk->arrayInfo()->size() == 0) {
      act_error_ctxt (stderr);
      fatal_error ("Port `%s': zero-length array creation not permitted",
		   getPortName (i));
    }
    
    ActId *tmp = new ActId (getPortName (i));
    tmp->Canonical (ux->CurScope());
    delete tmp;
  }

  /*-- XXX: create this, if necessary --*/
  if (dynamic_cast<Channel *>(this)) {
    InstType *x;
    Assert (parent, "Hmm...");
    x = ux->root();

    Chan *ch = dynamic_cast <Chan *>(x->BaseType());
    Assert (ch, "Hmm?!");
    Assert (ch->datatype(), "What?");
    ux->CurScope()->Add ("self", ch->datatype());
  }
  else if (dynamic_cast<Data *>(this)) {
    InstType *x;

    if (TypeFactory::isStructure (this)) {
      /* no self for structures */
    }
    else {
      Assert (parent, "Hmm...");
      x = ux->root();
      ux->CurScope()->Add ("self", x);
    }
  }
  else if (dynamic_cast<Function *>(this)) {
    InstType *x = dynamic_cast<Function *>(this)->getRetType();
    x = x->Expand (ns, ux->I);
    ux->CurScope()->Add ("self", x);
  }
  else {
    /* process, no this pointer */
  }

  /*-- expand body --*/
  if (b) {
    b->Expandlist (ns, ux->I);
  }

  /*-- expand macros --*/
  for (int i=0; i < A_LEN (um); i++) {
    A_NEW (ux->um, UserMacro *);
    A_NEXT (ux->um) = um[i]->Expand (ux, ns, ux->I, (is_proc == 1) ? 1 : 0);
    A_INC (ux->um);
  }

  ux->pending = 0;
  recursion_depth--;
  return ux;
}



Data *Data::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Data *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Data *>(ux);
  }

  xd = new Data (ux);
  delete ux;

  Assert (_ns->EditType (xd->name, xd) == 1, "What?");
  xd->is_enum = is_enum;

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    xd->methods[i] = chp_expand (methods[i], ns, xd->CurScope());
  }
  
  return xd;
}


Channel *Channel::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Channel *xc;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Channel *>(ux);
  }

  xc = new Channel (ux);
  delete ux;

  Assert (_ns->EditType (xc->name, xc) == 1, "What?");

  for (i=0; i < ACT_NUM_STD_METHODS; i++) {
    xc->methods[i] = chp_expand (methods[i], ns, xc->CurScope());
  }
  for (i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    xc->emethods[i] = expr_expand (emethods[i], ns, xc->CurScope(), 0);
  }

  return xc;
}


Function *Function::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Function *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit, 2);
  ux->CurScope()->mkFunction();

  if (cache_hit) {
    return dynamic_cast<Function *>(ux);
  }

  xd = new Function (ux);
  delete ux;

  Assert (_ns->EditType (xd->name, xd) == 1, "What?");

  xd->setRetType (ret_type->Expand (ns, xd->I));
  xd->chkInline();
  
  return xd;
}

Interface *Interface::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Interface *xd;
  UserDef *ux;
  int cache_hit;
  int i;

  ux = UserDef::Expand (ns, s, nt, u, &cache_hit);

  if (cache_hit) {
    return dynamic_cast<Interface *>(ux);
  }

  xd = new Interface (ux);
  delete ux;

  Assert (_ns->EditType (xd->name, xd) == 1, "What?");
  return xd;
}

PType *PType::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Assert (nt == 1, "What?");

  return TypeFactory::Factory()->NewPType (u[0].u.tt->Expand (ns, s));
}

const char *PType::getName ()
{
  if (!i) {
    return "ptype";
  }
  else if (!name) {
    char buf[10240];
    sprintf (buf, "ptype(");
    i->sPrint (buf+strlen (buf), 10240-strlen (buf));
    sprintf (buf+strlen(buf), ")");
    name = Strdup (buf);
  }
  return name;
}



Int *Int::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Int *ix;
  AExpr *ae;
  InstType *it;
  
  Assert (nt == 1, "What?");

  if (u[0].u.tp) {
    ae = u[0].u.tp->Expand (ns, s);
  }
  else {
    ae = new AExpr (const_expr (32));
  }
  it = ae->getInstType (s, NULL);
  if (!TypeFactory::isPIntType (it->BaseType()) ||
      it->arrayInfo()) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Expanding integer type; parameter is not an integer\n");
    fprintf (stderr, " parameter: ");
    ae->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  delete it;

  AExprstep *step = ae->stepper();

  ix = TypeFactory::Factory()->NewInt (kind, step->getPInt());

  step->step();
  Assert (step->isend(), "Hmm?");

  delete step;
  delete ae;

  return ix;
}

const char *Int::getName()
{
  const char *k2v;

  if (name) return name;
  
  if (kind == 0) {
    k2v = "int";
  }
  else if (kind == 1) {
    k2v = "ints";
  }
  else {
    k2v = "enum";
  }
  
  if (w == -1) {
    name = k2v;
  }
  else {
    char buf[1024];
    sprintf (buf, "%s<%d>", k2v, w);
    name = Strdup (buf);
  }
  return name;
}

const char *Chan::getName ()
{
  char buf[10240];
  if (name) return name;

  if (!p) {
    sprintf (buf, "chan");
  }
  else {
    sprintf (buf, "chan(");
    p->sPrint (buf+strlen (buf), 10239-strlen(buf));
    if (ack) {
      strcat (buf, ",");
      ack->sPrint (buf + strlen (buf), 10239 - strlen (buf));
    }
    strcat (buf, ")");
  }
  name = Strdup (buf);
  return name;
}


Chan *Chan::Expand (ActNamespace *ns, Scope *s, int nt, inst_param *u)
{
  Chan *cx;
  InstType *cp, *ack;

  Assert (nt == 1 || nt == 2, "Hmm");
  cp = u[0].u.tt->Expand (ns, s);
  if (TypeFactory::isParamType (cp)) {
    act_error_ctxt (stderr);
    fprintf (stderr, "Parameter to channel type is not a datatype\n");
    fprintf (stderr, " parameter: ");
    cp->Print (stderr);
    fprintf (stderr, "\n");
    exit (1);
  }
  ack = NULL;
  if (nt == 2) {
    ack = u[1].u.tt->Expand (ns, s);
    if (TypeFactory::isParamType (ack)) {
      act_error_ctxt (stderr);
      fprintf (stderr, "Parameter to channel type is not a datatype\n");
      fprintf (stderr, " parameter: ");
      cp->Print (stderr);
      fprintf (stderr, "\n");
      exit (1);
    }
  }
  cx = TypeFactory::Factory()->NewChan (cp, ack);
  if (cx->p != cp) {
    /*-- got a cached version --*/
    delete cp;
    if (ack) {
      delete ack;
    }
  }
  return cx;
}


void UserDef::PrintHeader (FILE *fp, const char *type)
{
  char buf[10240];
  int n;

  if (exported) {
    fprintf (fp, "export ");
  }
  n = getNumParams();
  if (!expanded && n > 0) {
    fprintf (fp, "template <");
    for (int i=0; i < n; i++) {
      InstType *it = getPortType (-(i+1));
      Array *a = it->arrayInfo();
      it->clrArray();
      it->Print (fp);
      fprintf (fp, " %s", getPortName (-(i+1)));
      if (a) {
	a->Print (fp);
      }
      it->MkArray (a);
      if (i != n-1) {
	fprintf (fp, "; ");
      }
    }
    fprintf (fp, ">\n");
  }
  fprintf (fp, "%s ", type);
  if (expanded) {
    ActNamespace::Act()->mfprintfproc (fp, this, 1);
    fprintf (fp, " ");
  }
  else {
    /* ok there is a possibility of a name conflict here but lets not
       mangle more stuff */
    fprintf (fp, "%s ", getName());
  }

  int skip_ports = 0;

  if (parent) {
    fprintf (fp, "<: ");
    if (!expanded || !TypeFactory::isUserType (parent)) {
      parent->Print (fp);
    }
    else {
      UserDef *u = dynamic_cast<UserDef *>(parent->BaseType());
      Assert (u, "what?");
      ActNamespace::Act()->mfprintfproc (fp, u);
      skip_ports = u->getNumPorts();
    }
    fprintf (fp, " ");
  }

  n = getNumPorts ();
  fprintf (fp, "(");
  if (skip_ports < n) {
    for (int i=skip_ports; i < n; i++) {
      InstType *it = getPortType (i);
      Array *a = it->arrayInfo();
      it->clrArray ();
      if (it->isExpanded() && TypeFactory::isUserType (it)) {
	it->sPrint (buf, 10240, 1);
	ActNamespace::Act()->mfprintf (fp, "%s", buf);
      }
      else {
	it->Print (fp);
      }
      fprintf (fp, " %s", getPortName (i));
      if (a) {
	a->Print (fp);
      }
      it->MkArray (a);
      if (i != n-1) {
	fprintf (fp, "; ");
      }
    }
  }
  fprintf (fp, ")");
}


/*------------------------------------------------------------------------
 *
 *  Returns first built-in type in the parent type hierarchy.
 *
 *     THIS SHOULD ONLY BE CALLED FOR user-defined channels and data
 *     types. 
 *
 *------------------------------------------------------------------------
 */
InstType *UserDef::root () const
{
  if (!parent) {
    return NULL;
  }
  if (TypeFactory::isUserType (parent)) {
    UserDef *ux = dynamic_cast<UserDef *> (parent->BaseType());
    return ux->root();
  }
  return parent;
}


/*------------------------------------------------------------------------
 *  Printing functions
 *------------------------------------------------------------------------
 */

void Channel::Print (FILE *fp)
{
  PrintHeader (fp, "defchan");
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  lang->Print (fp);

  int firstmeth = 1;

#define EMIT_METHOD_HEADER(id)			\
  do {						\
    if (methods[id]) {				\
      if (firstmeth) {				\
	fprintf (fp, "  methods {\n");		\
      }						\
      firstmeth = 0;				\
    }						\
  } while (0)

#define EMIT_METHOD_EXPRHEADER(id)		\
  do {						\
    if (emethods[id-ACT_NUM_STD_METHODS]) {	\
      if (firstmeth) {				\
	fprintf (fp, "  methods {\n");		\
      }						\
      firstmeth = 0;				\
    }						\
  } while (0)

#define EMIT_METHOD(id)						\
  do {								\
    EMIT_METHOD_HEADER(id);					\
    if (methods[id]) {						\
      fprintf (fp, "  %s {\n", act_builtin_method_name[id]);	\
      chp_print (fp, methods[id]);				\
      fprintf (fp, "}\n");					\
    }								\
  } while (0)

#define EMIT_METHODEXPR(id)						\
  do {									\
    EMIT_METHOD_EXPRHEADER(id);						\
    if (emethods[id-ACT_NUM_STD_METHODS]) {				\
      fprintf (fp, "  %s =", act_builtin_method_expr[id-ACT_NUM_STD_METHODS]); \
      print_expr (fp, emethods[id-ACT_NUM_STD_METHODS]);		\
      fprintf (fp, ";\n");						\
    }									\
  } while (0)

  for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
    EMIT_METHOD (i);
  }
  for (int i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    EMIT_METHODEXPR (i + ACT_NUM_STD_METHODS);
  }

  firstmeth = emitMacros (fp) ? 1 : firstmeth;
  
  if (!firstmeth) {
    fprintf (fp, "}\n");
  }
  fprintf (fp, "}\n\n");
}

void Data::Print (FILE *fp)
{
  PrintHeader (fp, "deftype");
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }
  
  lang->Print (fp);

  int firstmeth = 1;

  EMIT_METHOD(ACT_METHOD_SET);
  EMIT_METHOD(ACT_METHOD_GET);

  firstmeth = emitMacros (fp) ? 1 : firstmeth;

  if (!firstmeth) {
    fprintf (fp, " }\n");
  }
  fprintf (fp, "}\n\n");
}

void Function::Print (FILE *fp)
{
  PrintHeader (fp, "function");
  fprintf (fp, " : ");
  if (expanded) {
    if (TypeFactory::isUserType (getRetType())) {
      UserDef *u = dynamic_cast<UserDef *> (getRetType()->BaseType());
      ActNamespace::Act()->mfprintfproc (fp, u, 1);
    }
    else {
      getRetType()->Print (fp);
    }
  }
  fprintf (fp, "\n{\n");
  if (!expanded) {
    /* print act bodies */
    ActBody *bi;

    for (bi = b; bi; bi = bi->Next()) {
      bi->Print (fp);
    }
  }
  else {
    CurScope()->Print (fp);
  }

  /* print language bodies */
  lang->Print (fp);

  
  fprintf (fp, "}\n\n");
}


void UserDef::AppendBody (ActBody *x)
{
  if (b) {
    b->Append (x);
  }
  else {
    b = x;
  }
}


void Data::copyMethods (Data *d)
{
  for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = d->getMethod ((datatype_methods)i);
  }
}

void Channel::copyMethods (Channel *c)
{
  for (int i=0; i < ACT_NUM_STD_METHODS; i++) {
    methods[i] = c->getMethod ((datatype_methods)i);
  }
  for (int i=0; i < ACT_NUM_EXPR_METHODS; i++) {
    emethods[i] = c->geteMethod ((datatype_methods)(i+ACT_NUM_STD_METHODS));
  }
}


act_prs *UserDef::getprs ()
{
  return lang->getprs();
}

act_spec *UserDef::getspec ()
{
  return lang->getspec();
}

int UserDef::isEqual (const Type *t) const
{
  const UserDef *u = dynamic_cast<const UserDef *>(t);
  if (!u) return 0;
  if (u == this) return 1;
  return 0;
}

int Int::isEqual (const Type *t) const
{
  const Int *x = dynamic_cast<const Int *>(t);
  if (!x) return 0;
  if (x->kind == kind && x->w == w) return 1;
  return 0;
}
  
int Chan::isEqual (const Type *t) const
{
  const Chan *x = dynamic_cast<const Chan *>(t);
  if (!x) return 0;
  if (x == this) return 1;
  if (!p && !x->p) return 1;
  if (!p || !x->p) return 0;

  if (p->isExpanded()) {
    if (!p->isEqual (x->p)) return 0;
  }
  else {
    if (!p->isEqual (x->p, 1)) return 0;
  }

  if (!ack && !x->ack) return 1;
  if (!ack || !x->ack) return 0;

  if (ack->isExpanded()) {
    if (!ack->isEqual (x->ack)) return 0;
  }
  else {
    if (!ack->isEqual (x->ack, 1)) return 0;
  }
  return 1;
}



int UserDef::isLeaf ()
{
  ActInstiter i(I);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = (*i);
    if (TypeFactory::isProcessType (vx->t)) {
      return 0;
    }
  }
  return 1;
}

static void _run_chp (Scope *s, act_chp_lang_t *c)
{
  int loop_cnt = 0;
  
  if (!c) return;
  switch (c->type) {
  case ACT_CHP_COMMA:
  case ACT_CHP_SEMI:
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      _run_chp (s, (act_chp_lang_t *) list_value (li));
    }
    break;

  case ACT_CHP_COMMALOOP:
  case ACT_CHP_SEMILOOP:
    {
      int ilo, ihi;
      ValueIdx *vx;
      act_syn_loop_setup (NULL, s, c->u.loop.id, c->u.loop.lo, c->u.loop.hi,
			  &vx, &ilo, &ihi);

      for (int iter=ilo; iter <= ihi; iter++) {
	s->setPInt (vx->u.idx, iter);
	_run_chp (s, c->u.loop.body);
      }
      act_syn_loop_teardown (NULL, s, c->u.loop.id, vx);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
    {
      act_chp_gc_t *gc;
      for (gc = c->u.gc; gc; gc = gc->next) {
	Expr *guard;
	if (gc->id) {
	  ValueIdx *vx;
	  int ilo, ihi;

	  act_syn_loop_setup (NULL, s, gc->id, gc->lo, gc->hi,
			      &vx, &ilo, &ihi);
	
	  for (int iter=ilo; iter <= ihi; iter++) {
	    s->setPInt (vx->u.idx, iter);
	    guard = expr_expand (gc->g, NULL, s);
	    if (!guard || guard->type == E_TRUE) {
	      /* guard is true */
	      _run_chp (s, gc->s);
	      act_syn_loop_teardown (NULL, s, gc->id, vx);
	      return;
	    }
	    if (!expr_is_a_const (guard)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Guard is not a constant expression?");
	    }
	  }
	  act_syn_loop_teardown (NULL, s, gc->id, vx);
	}
	else {
	  guard = expr_expand (gc->g, NULL, s);
	  if (!guard || guard->type == E_TRUE) {
	    _run_chp (s, gc->s);
	    return;
	  }
	  if (!expr_is_a_const (guard)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Guard is not a constant expression?");
	  }
	}
      }
      act_error_ctxt (stderr);
      fatal_error ("In a function call: all guards are false!");
    }
    break;

  case ACT_CHP_LOOP:
    while (1) {
      loop_cnt++;
      for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
	Expr *guard;
	if (gc->id) {
	  ValueIdx *vx;
	  int ilo, ihi;
	  
	  act_syn_loop_setup (NULL, s, gc->id, gc->lo, gc->hi,
			      &vx, &ilo, &ihi);
	
	  for (int iter=ilo; iter <= ihi; iter++) {
	    s->setPInt (vx->u.idx, iter);
	    guard = expr_expand (gc->g, NULL, s);
	    if (!guard || guard->type == E_TRUE) {
	      /* guard is true */
	      _run_chp (s, gc->s);
	      act_syn_loop_teardown (NULL, s, gc->id, vx);
	      goto resume;
	    }
	    if (!expr_is_a_const (guard)) {
	      act_error_ctxt (stderr);
	      fatal_error ("Guard is not a constant expression?");
	    }
	  }
	  act_syn_loop_teardown (NULL, s, gc->id, vx);
	}
	else {
	  guard = expr_expand (gc->g, NULL, s);
	  if (!guard || guard->type == E_TRUE) {
	    _run_chp (s, gc->s);
	    goto resume;
	  }
	  if (!expr_is_a_const (guard)) {
	    act_error_ctxt (stderr);
	    fatal_error ("Guard is not a constant expression?");
	  }
	}
      }
      /* all guards false */
      return;
    resume:
      if (loop_cnt > Act::max_loop_iterations) {
	fatal_error ("# of loop iterations exceeded limit (%d)", Act::max_loop_iterations);
      }
      ;
    }
    break;

  case ACT_CHP_DOLOOP:
    {
      Expr *guard;
      Assert (c->u.gc->next == NULL, "What?");
      do {
	_run_chp (s, c->u.gc->s);
	guard = expr_expand (c->u.gc->g, NULL, s);
      } while (!guard || guard->type == E_TRUE);
      if (!expr_is_a_const (guard)) {
	act_error_ctxt (stderr);
	fatal_error ("Guard is not a constant expression?");
      }
    }
    break;

  case ACT_CHP_SKIP:
    break;

  case ACT_CHP_ASSIGN:
    {
      Expr *e = expr_expand (c->u.assign.e, NULL, s);
      ActId *id = expand_var_write (c->u.assign.id, NULL, s);
      if (!expr_is_a_const (e)) {
	act_error_ctxt (stderr);
	fatal_error ("Assignment: expression is not a constant?");
      }
      AExpr *ae = new AExpr (e);
      s->BindParam (id, ae);
      delete ae;
      delete id;
    }      
    break;
    
  case ACT_CHP_SEND:
    act_error_ctxt (stderr);
    fatal_error ("Channels in a function?");
    break;
    
  case ACT_CHP_RECV:
    act_error_ctxt (stderr);
    fatal_error ("Channels in a function?");
    break;

  case ACT_CHP_FUNC:
    /* built-in functions; skip */
    break;
    
  default:
    break;
  }
}

Expr *Function::eval (ActNamespace *ns, int nargs, Expr **args)
{
  Assert (nargs == getNumParams(), "What?");
  
  for (int i=0; i < nargs; i++) {
      Assert (expr_is_a_const (args[i]), "Argument is not a constant?");
  }

  /* 
     now we allocate all the parameters within the function scope
     and bind them to the specified values.
  */
    
  /* evaluate function!
     1. Bind parameters 
  */

  if (pending) {
    fatal_error ("Sorry, recursive functions (`%s') not supported.",
		 getName());
  }

  /* XXX we could cache the allocated state, maybe later...*/
  
  I->FlushExpand ();
  pending = 1;
  expanded = 1;

  ValueIdx *vx;
      
  for (int i=0; i < getNumParams(); i++) {
    InstType *it;
    const char *name;
    it = getPortType (-(i+1));
    name = getPortName (-(i+1));
    
    I->Add (name, it);
    vx = I->LookupVal (name);
    Assert (vx, "Hmm");
    vx->init = 1;
    if (TypeFactory::isPIntType (it)) {
      vx->u.idx = I->AllocPInt();
    }
    else if (TypeFactory::isPBoolType (it)) {
      vx->u.idx = I->AllocPBool();
    }
    else if (TypeFactory::isPRealType (it)) {
      vx->u.idx = I->AllocPReal();
    }
    else {
      fatal_error ("Invalid type in function signature");
    }
    AExpr *ae = new AExpr (args[i]);
    I->BindParam (name, ae);
    delete ae;
  }

  I->Add ("self", getRetType ()->Expand (ns, I));
  vx = I->LookupVal ("self");
  Assert (vx, "Hmm");
  vx->init = 1;
  if (TypeFactory::isPIntType (getRetType())) {
    vx->u.idx = I->AllocPInt();
  }
  else if (TypeFactory::isPBoolType (getRetType())) {
    vx->u.idx = I->AllocPBool();
  }
  else if (TypeFactory::isPRealType (getRetType())) {
    vx->u.idx = I->AllocPReal();
  }
  else {
    fatal_error ("Invalid return type in function signature");
  }
  

  /* now run the chp body */
  act_chp *c = NULL;
  
  if (b) {
    ActBody *btmp;

    for (btmp = b; btmp; btmp = btmp->Next()) {
      ActBody_Lang *l;
      if (!(l = dynamic_cast<ActBody_Lang *>(btmp))) {
	btmp->Expand (ns, I);
      }
      else {
	Assert (l->gettype() == ActBody_Lang::LANG_CHP, "What?");
	c = (act_chp *)l->getlang();
      }
    }
  }
  
  /* run the chp */
  Assert (c, "Isn't this required?!");
  _run_chp (I, c->c);

  pending = 0;

  Expr *ret;

  if (TypeFactory::isPIntType (getRetType())) {
    if (I->issetPInt (vx->u.idx)) {
      ret = const_expr (I->getPInt (vx->u.idx));
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("self is not assigned!");
    }
  }
  else if (TypeFactory::isPBoolType (getRetType())) {
    if (I->issetPBool (vx->u.idx)) {
      ret = const_expr_bool (I->getPBool (vx->u.idx));
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("self is not assigned!");
    }
  }
  else if (TypeFactory::isPRealType (getRetType())) {
    if (I->issetPReal (vx->u.idx)) {
      ret = const_expr_real (I->getPReal (vx->u.idx));
    }
    else {
      act_error_ctxt (stderr);
      fatal_error ("self is not assigned!");
      ret = NULL;
    }
  }
  else {
    fatal_error ("Invalid return type in function signature");
    ret = NULL;
  }
  return ret;
}

void Data::getStructCount (int *nbools, int *nints)
{
  if (!TypeFactory::isStructure (this) || !isExpanded()) {
    *nbools = -1;
    *nints = -1;
    return;
  }
  *nbools = 0;
  *nints = 0;
  _get_struct_count (nbools, nints);
}

void Data::_get_struct_count (int *nb, int *ni)
{
  for (int i=0; i < getNumPorts(); i++ ) {
    InstType *it = getPortType (i);
    int sz;
    if (it->arrayInfo()) {
      sz = it->arrayInfo()->size();
    }
    else {
      sz = 1;
    }
    if (TypeFactory::isIntType (it)) {
      *ni += sz;
    }
    else if (TypeFactory::isBoolType (it)) {
      *nb += sz;
    }
    else if (TypeFactory::isStructure (it)) {
      int tmpb, tmpi;
      Data *d = dynamic_cast<Data *>(it->BaseType());
      Assert (d, "Hmm");
      d->getStructCount (&tmpb, &tmpi);
      *nb += sz*tmpb;
      *ni += sz*tmpi;
    }
    else {
      Data *d = dynamic_cast<Data *> (it->BaseType());
      Assert (d, "Hmm");
      if (TypeFactory::isIntType (d->root())) {
	*ni += sz;
      }
      else if (TypeFactory::isBoolType (d->root())) {
	*nb += sz;
      }
      else {
	Assert (0, "structure/data invariant violated!");
      }
    }
  }
}

int Data::getStructOffset (ActId *field, int *sz)
{
  Assert (TypeFactory::isStructure (this), "What?");
  if (!field) return -1;
  
  for (int i=0; i < getNumPorts(); i++) {
    if (strcmp (field->getName(), getPortName (i)) == 0) {
      if (!field->Rest()) {
	if (sz) {
	  InstType *tmp = getPortType (i);
	  if (TypeFactory::isStructure (tmp)) {
	    Data *xd = dynamic_cast<Data *> (tmp->BaseType());
	    int nb, ni;
	    xd->getStructCount (&nb, &ni);
	    *sz = nb + ni;
	  }
	  else {
	    *sz = 1;
	  }
	  if (tmp->arrayInfo()) {
	    *sz = *sz * tmp->arrayInfo()->size();
	  }
	}
	return i;
      }
      else {
	InstType *it = getPortType (i);
	Data *d;
	int x;
	Assert (TypeFactory::isStructure (it), "What?");
	Assert (it->arrayInfo() == NULL, "What?");
	d = dynamic_cast <Data *> (it->BaseType());
	x = d->getStructOffset (field->Rest(), sz);
	if (x == -1) {
	  return -1;
	}
	return i + x;
      }
    }
  }
  return -1;
}

ActId **Data::getStructFields (int **types)
{
  Assert (TypeFactory::isStructure (this), "What?");
  int nb, ni;
  getStructCount (&nb, &ni);

  ActId **ret;
  int idx;
  MALLOC (ret, ActId *, nb + ni);
  MALLOC (*types, int, nb + ni);

  idx = 0;

  _get_struct_fields (ret, *types, &idx, NULL);
  Assert (idx == nb + ni, "What?");

  return ret;
}

void Data::_get_struct_fields (ActId **a, int *types, int *pos, ActId *prefix)
{
  Assert (TypeFactory::isStructure (this), "What?!");
  
  for (int i=0; i < getNumPorts(); i++) {
    if (TypeFactory::isStructure (getPortType (i))) {
      Data *d = dynamic_cast<Data *> (getPortType(i)->BaseType());
      _get_struct_fields (a, pos, types, prefix);
    }
    else {
      ActId *tmp = new ActId (getPortName (i));
      if (prefix) {
	a[*pos] = prefix->Clone();
	a[*pos]->Tail()->Append (tmp);
      }
      else {
	a[*pos] = tmp;
      }
      if (TypeFactory::isBoolType (getPortType (i))) {
	types[*pos] = 0;
      }
      else {
	Assert (TypeFactory::isIntType (getPortType (i)), "Structure invariant violated?");
	types[*pos] = 1;
      }
      *pos = *pos + 1;
    }
  }
}

int Function::isExternal ()
{
  return !getlang() || !getlang()->getchp();
}

/*
 *  return 1 for input, 2 for output
 */
int Channel::chanDir (ActId *id, int isinput)
{
  int iosend = 0, iorecv = 0;

  InstType *it = I->Lookup (id->getName());

  Assert (it, "Fragmented piece not in the port list?");
  
  int dir = it->getDir ();
  if (dir == Type::INOUT) {
    if (isinput) {
      return 1;
    }
    else {
      return 2;
    }
  }
  else if (dir == Type::OUTIN) {
    if (isinput) {
      return 2;
    }
    else {
      return 1;
    }
  }
  else if (dir == Type::IN) {
    if (isinput) {
      return 3;
    }
    else {
      return 0;
    }
  }
  else if (dir == Type::OUT) {
    if (isinput) {
      return 0;
    }
    else {
      return 3;
    }
  }

  /*-- now look through methods --*/
  if (methods[ACT_METHOD_SET]) {
    iosend = act_hse_direction (methods[ACT_METHOD_SET], id);
  }
  if (!iosend && methods[ACT_METHOD_SEND_REST]) {
    iosend = act_hse_direction (methods[ACT_METHOD_SEND_REST], id);
  }

  dir = 0;

  if (isinput) {
    if (iosend & 1) {
      dir |= 2;
    }
  }
  else {
    if (iosend & 2) {
      dir |= 2;
    }
  }
  
  if (methods[ACT_METHOD_GET]) {
    iorecv = act_hse_direction (methods[ACT_METHOD_GET], id);
  }
  if (!iorecv && methods[ACT_METHOD_RECV_REST]) {
    iorecv = act_hse_direction (methods[ACT_METHOD_RECV_REST], id);
  }

  if (isinput) {
    if (iorecv & 1) {
      dir |= 1;
    }
  }
  else {
    if (iorecv & 2) {
      dir |= 1;
    }
  }
  
  return dir;
}


int Channel::mustbeActiveSend ()
{
  if (geteMethod (ACT_METHOD_SEND_PROBE) &&
      geteMethod (ACT_METHOD_RECV_PROBE)) {
    fprintf (stderr, "Channel type: %s", getName());
    fatal_error ("Channel cannot define probes on both send and receive");
  }
  if (isBiDirectional()) {
    if (geteMethod (ACT_METHOD_SEND_PROBE)) {
      fprintf (stderr, "Channel type: %s", getName());
      fatal_error ("Bidirectional channel cannot define probe on send");
    }
    return 1;
  }
  if (geteMethod (ACT_METHOD_SEND_PROBE)) {
    return 0;
  }
  else if (geteMethod (ACT_METHOD_RECV_PROBE)) {
    return 1;
  }
  else {
    return -1;
  }
}

int Channel::mustbeActiveRecv ()
{
  int x = mustbeActiveSend();
  if (x == 0) {
    return 1;
  }
  else if (x == 1) {
    return 0;
  }
  else {
    return -1;
  }
}


int UserDef::emitMacros (FILE *fp)
{
  if (A_LEN (um) > 0) {
    for (int i=0; i < A_LEN (um); i++) {
      UserMacro *u = um[i];
      Assert (u, "Hmm");
      u->Print (fp);
      fprintf (fp, "\n");
    }
    return 1;
  }
  return 0;
}

/*--- macros ---*/


UserMacro *UserDef::newMacro (const char *name)
{
  name = string_cache (name);
  for (int i=0; i < A_LEN (um); i++) {
    if (strcmp (um[i]->getName(), name) == 0) {
      return NULL;
    }
  }
  A_NEW (um, UserMacro *);
  A_NEXT (um) = new UserMacro (this, name);
  A_INC (um);
  return um[A_LEN(um)-1];
}

UserMacro *UserDef::getMacro (const char *name)
{
  name = string_cache (name);
  for (int i=0; i < A_LEN (um); i++) {
    if (strcmp (um[i]->getName(), name) == 0) {
      return um[i];
    }
  }
  return NULL;
}


/* copy over userdef */
UserDef::UserDef (UserDef *x)
{
  A_INIT (um);
  MkCopy (x);
}
