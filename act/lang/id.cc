/*************************************************************************
 *
 *  This file is part of the ACT library
 *
 *  Copyright (c) 2018-2019 Rajit Manohar
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
#include <act/types.h>
#include <act/inst.h>
#include <act/body.h>
#include <act/value.h>
#include <string.h>
#include <ctype.h>

static void print_id (act_connection *c);

/*------------------------------------------------------------------------
 *
 * Identifiers
 *
 *------------------------------------------------------------------------
 */
ActId::ActId (const char *s, Array *_a)
{
  name = string_create (s);
  a = _a;
  next = NULL;
}

ActId::~ActId ()
{
  string_free (name);
  if (a) {
    delete a;
  }
  if (next) {
    delete next;
  }
  next = NULL;
  a = NULL;
  name = NULL;
}
   


ActId *ActId::Clone ()
{
  ActId *ret;
  Array *aclone;

  if (a) {
    aclone = a->Clone();
  }
  else {
    aclone = NULL;
  }

  ret = new ActId (string_char (name), aclone);

  if (next) {
    ret->next = next->Clone ();
  }

  return ret;
}

ActId *ActId::Expand (ActNamespace *ns, Scope *s)
{
  ActId *ret;
  Array *ax;

  if (a) {
    ax = a->ExpandRef (ns, s);
  }
  else {
    ax = NULL;
  }

  ret = new ActId (string_char (name), ax);

  if (next) {
    ret->next = next->Expand (ns, s);
  }

#if 0
  fprintf (stderr, "expanding id: ");
  this->Print (stderr, NULL);
  fprintf (stderr, " -> ");
  ret->Print (stderr, NULL);
  fprintf (stderr, "\n");
#endif

  return ret;
}

ActId *ActId::ExpandCHP (ActNamespace *ns, Scope *s)
{
  ActId *ret;
  Array *ax;

  if (a) {
    ax = a->ExpandRefCHP (ns, s);
  }
  else {
    ax = NULL;
  }

  ret = new ActId (string_char (name), ax);

  if (next) {
    ret->next = next->ExpandCHP (ns, s);
  }

#if 0
  fprintf (stderr, "expanding id: ");
  this->Print (stderr, NULL);
  fprintf (stderr, " -> ");
  ret->Print (stderr, NULL);
  fprintf (stderr, "\n");
#endif

  return ret;
}


/*------------------------------------------------------------------------
 *
 *  Evaluate identifier: It either returns an evaluated identifier
 *  (i.e. all items expanded out), or in the case of something that
 *  isn't an lval, the value of the parameter.
 *
 *  Return types are:
 *    for lval: E_VAR
 *    otherwise: E_VAR (for non-parameter types)
 *               E_TRUE, E_FALSE, E_TYPE, E_REAL, E_INT,
 *               E_ARRAY, E_SUBRANGE -- for parameter types
 *
 *------------------------------------------------------------------------
 */
Expr *ActId::Eval (ActNamespace *ns, Scope *s, int is_lval, int is_chp)
{
  InstType *it;
  ActId *id;
  Expr *ret;
  Type *base;
  int is_ns = isNamespace();

  NEW (ret, Expr);

  if (is_ns) {
    ActNamespace *curns = getNamespace();
    s = curns->CurScope();
    it = s->Lookup (Rest()->getName());
    if (!it) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Could not find namespace global id");
    }
  }
  else {
    do {
      it = s->Lookup (getName ());
      if (!it) {
	s = s->Parent ();
      }
    } while (!it && s);
  }
  if (!s) {
    act_error_ctxt (stderr);
    fprintf (stderr, " id: ");
    this->Print (stderr);
    fprintf (stderr, "\n");
    fatal_error ("Identifer conditionally created, but used when it does not exist");
    //fatal_error ("Not found. Should have been caught earlier...");
  }
  Assert (it->isExpanded (), "Hmm...");

  if (is_ns) {
    id = this->Rest();
  }
  else {
    id = this;
  }

  do {

#if 0
    printf ("checking: ");
    id->Print (stdout);
    printf (" in [%lx] ", it->BaseType());
    fflush (stdout);
    it->Print (stdout);
    printf ("\n");
#endif
    
    /* insttype is "it";
       scope is "s"
       id is "id"

       check array deref:
       if the id has one, then:
          -- either the type is an array type, yay
	  -- otherwise error
    */
    if (id->arrayInfo() && !it->arrayInfo()) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Array de-reference without array type.");
    }
    if (it->arrayInfo() && !id->arrayInfo() && id->Rest()) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Missing array dereference for ``%s''.", id->getName());
    }

    if (id->arrayInfo()) {
      if (!id->arrayInfo()->isDeref() && id->Rest()) {
	act_error_ctxt (stderr);
	fprintf (stderr, " id: ");
	this->Print (stderr);
	fprintf (stderr, "; deref: ");
	id->arrayInfo()->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Using `.' for an array, not an array de-reference");
      }
      if (id->arrayInfo()->isDeref() &&
	  id->arrayInfo()->isDynamicDeref()) {
	if (is_chp) {
	  /* ok */
	}
	else {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " id: ");
	  this->Print (stderr);
	  fprintf (stderr, "; deref: ");
	  id->arrayInfo()->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Dynamic de-references only permited in CHP/HSE");
	}
      }
      else {
	if (!it->arrayInfo()->Validate (id->arrayInfo())) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " id: ");
	  this->Print (stderr);
	  fprintf (stderr, "\n type: ");
	  it->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Dereference out of range");
	}
      }
    }

    if (id->Rest()) {
      UserDef *u;

#if 0      
      printf ("Type:  [it=");
      it->Print (stdout);
      printf ("]\n");
      printf (" rest: ");
      id->Rest()->Print (stdout);
      printf ("\n");
#endif      
    
      Assert (it->isExpanded(), "This should be expanded");
      
      u = dynamic_cast<UserDef *>(it->BaseType ());
      Assert (u, "This should have been caught earlier!");
    
      /* WWW: here we would have to check the array index for relaxed
	 parameters */

      /* NOW: if it is an interface type, then we have to find the map
	 and change this part 
      */
      if (it->getIfaceType()) {
	/* extract the real type */
	InstType *itmp = it->getIfaceType();
	Assert (itmp, "What?");
	Process *proc = dynamic_cast<Process *> (it->BaseType());
	Assert (proc, "What?");
	list_t *map = proc->findMap (itmp);
	if (!map) {
	  fatal_error ("Missing interface `%s' from process `%s'?",
		       itmp->BaseType()->getName(), proc->getName());
	}
	listitem_t *li;
#if 0
	printf ("ID rest: ");
	id->Rest()->Print(stdout);
	printf ("\n");
#endif	
	
	for (li = list_first (map); li; li = list_next (li)) {
	  char *from = (char *)list_value (li);
	  char *to = (char *)list_value (list_next(li));
	  if (strcmp (id->Rest()->getName(), from) == 0) {
	    ActId *nid = new ActId (to);
	    if (id->Rest()->arrayInfo()) {
	      nid->setArray (id->Rest()->arrayInfo());
	    }
	    /* XXX: ID CACHING */
	    nid->Append (id->Rest()->Rest());
	    id = nid;
	    break;
	  }
	  li = list_next (li);
	}
	if (!li) {
	  fatal_error ("Map for interface `%s' doesn't contain `%s'",
		       itmp->BaseType()->getName(), id->Rest()->getName());
	}
#if 0	
	printf ("NEW ID rest: ");
	id->Print(stdout);
	printf ("\n");
#endif	
      }
      else {
	id = id->Rest ();
      }
      s = u->CurScope ();
      it = s->Lookup (id->getName ());
    }
    else {
      break;
    }
  } while (1);

  /* identifier is "id"
     scope is "s"
     type is "it"
     any array index has been validated
  */

  base = it->BaseType ();

  /* now, verify that the type is a parameter v/s not a parameter */
  if (TypeFactory::isParamType (base)) {
    ValueIdx *vx = s->LookupVal (id->getName ());
    int offset = 0;
    if (!vx->init && !is_lval) {
      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Uninitialized identifier");
    }

    /* now we check for each type */
    if (it->arrayInfo() && id->arrayInfo()) {
      if (!id->arrayInfo()->isDynamicDeref()) {
	offset = it->arrayInfo()->Offset (id->arrayInfo());
	if (offset == -1) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " id: ");
	  this->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Index is out of range");
	}
	Assert (offset != -1, "Hmm...");
      }
    }

    if (is_lval) {
      ret->type = E_VAR;
      ret->u.e.l = (Expr *)this;
      ret->u.e.r = (Expr *)it;
      return ret;
    }

    if (it->arrayInfo() &&
	(!id->arrayInfo() || !id->arrayInfo()->isDeref())) {
      int k, idx;
      int sz;

      if (!id->arrayInfo()) {
	sz = it->arrayInfo()->size();
      }
      else {
	sz = id->arrayInfo()->size();
      }
      /* check that the entire array is set! */
      for (k=0; k < sz; k++) {
	if (id->arrayInfo()) {
	  Assert (offset != -1, "what?");
	  idx = vx->u.idx + offset + k;
	}
	else {
	  /* dense array, simple indexing */
	  idx = vx->u.idx + k;
	}
	/* check the appropriate thing is set */
	if ((TypeFactory::isPIntType (base) && !s->issetPInt (idx)) ||
	    (TypeFactory::isPIntsType (base) && !s->issetPInts (idx)) ||
	    (TypeFactory::isPBoolType (base) && !s->issetPBool (idx)) ||
	    (TypeFactory::isPRealType (base) && !s->issetPReal (idx)) ||
	    (TypeFactory::isPTypeType (base) && !s->issetPType (idx))) {
	  act_error_ctxt (stderr);
	  fprintf (stderr, " id: ");
	  this->Print (stderr);
	  fprintf (stderr, "\n");
	  fatal_error ("Array has an uninitialized element");
	}
      }
      if (!id->arrayInfo()) {
	ret->type = E_ARRAY;
	ret->u.e.l = (Expr *) vx;
	ret->u.e.r = (Expr *) s;
      }
      else {
	ret->type = E_SUBRANGE;
	ret->u.e.l = (Expr *) vx;
	NEW (ret->u.e.r, Expr);
	ret->u.e.r->u.e.l = (Expr *)s;
	ret->u.e.r->u.e.r = (Expr *)id->arrayInfo()->Clone ();
      }
      return ret;
    }
    if ((TypeFactory::isPIntType(base) && !s->issetPInt (vx->u.idx + offset)) ||
	(TypeFactory::isPIntsType(base) && !s->issetPInts (vx->u.idx + offset)) ||
	(TypeFactory::isPBoolType(base) && !s->issetPBool (vx->u.idx + offset)) ||
	(TypeFactory::isPRealType(base) && !s->issetPReal (vx->u.idx + offset)) ||
	(TypeFactory::isPTypeType(base) && !s->issetPType (vx->u.idx + offset))) {

      act_error_ctxt (stderr);
      fprintf (stderr, " id: ");
      this->Print (stderr);
      fprintf (stderr, "\n");
      fatal_error ("Uninitialized value");
    }
    if (TypeFactory::isPIntType (base)) {
      Expr *tmp;

      ret->type = E_INT;
      ret->u.v = s->getPInt (offset + vx->u.idx);
      ret->u.v_extra = NULL;

      tmp = TypeFactory::NewExpr (ret);
      FREE (ret);
      ret = tmp;

      return ret;
    }
    else if (TypeFactory::isPIntsType (base)) {
      Expr *tmp;

      ret->type = E_INT;
      ret->u.v = s->getPInts (offset + vx->u.idx);
      ret->u.v_extra = NULL;

      tmp = TypeFactory::NewExpr (ret);
      FREE (ret);
      ret = tmp;
      
      return ret;
    }
    else if (TypeFactory::isPBoolType (base)) {
      Expr *tmp;

      if (s->getPBool (offset + vx->u.idx)) {
	ret->type = E_TRUE;
      }
      else {
	ret->type = E_FALSE;
      }

      tmp = TypeFactory::NewExpr (ret);
      FREE (ret);
      ret = tmp;

      return ret;
    }
    else if (TypeFactory::isPRealType (base)) {
      ret->type = E_REAL;
      ret->u.f = s->getPReal (offset + vx->u.idx);
      return ret;
    }
    else if (TypeFactory::isPTypeType (base)) {
      ret->type = E_TYPE;
      ret->u.e.l = (Expr *) s->getPType (offset + vx->u.idx);
      return ret;
    }
    else {
      Assert (0, "Should not be here");
    }
  }
  else {
    /* all this for nothing! */
    ret->type = E_VAR;
    ret->u.e.l = (Expr *)this;
    ret->u.e.r = (Expr *)it;
  }
  return ret;
}


int ActId::isRange ()
{
  ActId *id;

  id = this;
  while (id) {
    if (id->a && !id->a->isDeref()) {
      return 1;
    }
    id = id->next;
  }
  return 0;
}

void ActId::Append (ActId *id)
{
  Assert (!next, "ActId::Append() called with non-NULL next field");
  next = id;
}

void ActId::Print (FILE *fp, ActId *end, int style)
{
  char buf[10240];
  buf[0] = '\0';
  sPrint (buf, 10240, end, style);
  fprintf (fp, "%s", buf);
}

#define PRINT_STEP				\
  do {						\
    len = strlen (buf+k);			\
    k += len;					\
    sz -= len;					\
    if (sz <= 1) return;			\
  } while (0)

void ActId::sPrint (char *buf, int sz, ActId *end, int style)
{
  ActId *start = this;
  int k = 0;
  int len;
  int ns = isNamespace();

  while (start && start != end) {
    if (start != this) {
      if (!ns) {
	snprintf (buf+k, sz, ".");
	PRINT_STEP;
      }
      ns = 0;
    }
    snprintf (buf+k, sz, "%s", string_char(start->name));
    PRINT_STEP;
    if (start->a) {
      start->a->sPrint (buf+k, sz, style);
      PRINT_STEP;
    }
    start = start->next;
  }
}


list_t *_act_create_connection_stackidx (act_connection *c, act_connection **cret)
{
  list_t *l = list_new ();

  while (c->parent) {
    int x = c->myoffset ();
    stack_push (l, (void*)(long)x);
    c = c->parent;
  }
  Assert (c->vx, "Huh?");
  if (cret) {
    *cret = c;
  }
  return l;
}


/*
  ux = user-defined type whose ports have to be imported
  cx = connection pointer for the instance
  rootname = root of port
  stk = stack of traversal
*/
static act_connection *_find_corresponding_slot (UserDef *ux,
						 act_connection *cx,
						 const char *rootname,
						 list_t *stk)
{
  /* this MUST BE ANOTHER PORT! Find out where it starts */
  int oport;
  InstType *optype;
  int array_done;
  act_connection *pcx;
	
  oport = ux->FindPort (rootname);
  if (oport <= 0) {
    fprintf (stderr, "Looking for port %s in %s\n", rootname, ux->getName());
    Assert (oport > 0, "Port not found?");
  }
  oport--;
  optype = ux->getPortType (oport);
  array_done = 0;

  /* now we have to find the *corresponding* imported
     connection; that is  */
  pcx = cx->getsubconn (oport, ux->getNumPorts());
  if (!pcx->vx) {
    pcx->vx = ux->CurScope()->LookupVal (ux->getPortName (oport));
  }
  Assert (pcx->vx &&
	  pcx->vx == 
	  ux->CurScope()->LookupVal (ux->getPortName (oport)), "Hmm");

  listitem_t *li;
  
  for (li = list_first (stk); li; li = list_next (li)) {
    int pos = (long)list_value (li);
    if (!array_done && optype->arrayInfo()) {
      pcx = pcx->getsubconn (pos, optype->arrayInfo()->size());
      array_done = 1;
    }
    else {
      UserDef *oux = dynamic_cast<UserDef *>(optype->BaseType());
      Assert (oux, "Hmm.");
      pcx = pcx->getsubconn (pos, oux->getNumPorts());
      if (!pcx->vx) {
	pcx->vx = oux->CurScope()->LookupVal (oux->getPortName (pos));
      }
      Assert (pcx->vx &&
	      pcx->vx ==
	      oux->CurScope()->LookupVal (oux->getPortName (pos)),
	      "What?");
      optype = oux->getPortType (pos);
      array_done = 0;
    }
  }
  return pcx;
}

static void dump_conn (act_connection *c)
{
  act_connection *tmp, *root;

  root = c;
  while (root->up) root = root->up;

  tmp = c;

  printf ("conn: ");
  do {
    print_id (tmp);
    if (tmp == root) {
      printf ("*");
    }
    printf (" , ");
    tmp = tmp->next;
  } while (tmp != c);
  printf("\n");
}


/*
  replicate connection px into cx, in the context of user-defined type
  ux
*/
static void _import_conn_rec (act_connection *cxroot,
			      act_connection *cx, act_connection *px,
			      UserDef *ux)
{
  if (!px->isPrimary()) {
    /* have to do something */
    act_connection *ppx = px->primary();
    Assert (ppx != px, "What");

#if 0
    printf ("Importing connections up\n");
    printf (" From: %s\n", ux->getName());
    printf (" cxroot: "); print_id (cxroot);
    printf ("\n cx: "); print_id (cx);
    printf ("; px: "); print_id (px);
    printf ("\n  "); dump_conn (ppx);
#endif    
    
    if (ppx->isglobal()) {
      /* ok, easy */
      _act_mk_raw_connection (ppx, cx);
    }
    else {
      list_t *l;
      const char *name;

      l = _act_create_connection_stackidx (ppx, &ppx);
      name = ppx->vx->getName();

      ppx = _find_corresponding_slot (ux, cxroot, name, l);
      _act_mk_raw_connection (ppx, cx);

      list_free (l);
    }

#if 0
    printf ("-- after importing connections up");
    printf ("\n cx: "); print_id (cx);
    printf (";  "); dump_conn (cx);
#endif    
  }
  else if (px->hasSubconnections()) {
    /* have to do something else */
    act_connection *tmp2;
    int ct = px->getctype ();
    Assert (ct == 0 || ct == 1, "Hmm");
    /* 0 = array; 1 = userdef */

    //return; /* FIX THIS */
    
    for (int i=0; i < px->numSubconnections(); i++) {
      if (px->a[i]) {
	if (px->a[i]->isPrimary() && !px->a[i]->hasSubconnections())
	  continue;
	tmp2 = cx->getsubconn (i, px->numSubconnections());
	if (!tmp2->vx) {
	  tmp2->vx = px->a[i]->vx;
	}
	_import_conn_rec (cxroot, tmp2, px->a[i], ux);
      }
    }
  }
}



/* cx = root of instance valueidx
   ux = usertype
   a  = array info of the type (NULL if not present)

   cx is a connection entry of type "(ux,a)"

   So now if a port of cx is connected to something else, then 
   cx[0], ... cx[sz(a)-1]'s corresponding port should be connected
   to it as well.
*/
static void _import_connections (act_connection *cx, UserDef *ux, Array *a)
{
  Scope *us = ux->CurScope();
  int sz = a ? a->size() : 0;

  for (int i=0; i < ux->getNumPorts(); i++) {
    const char *port = ux->getPortName (i);

    /* clone port connections, if any */
    ValueIdx *pvx = us->LookupVal (port);
    Assert (pvx, "Port missing from local scope?!");
    act_connection *pcx = pvx->connection();

    if (!pvx->hasConnection()) {
      /* pristine port, don't have to worry about it! */
      continue;
    }

    /* port has a connection. but it is primary and 
       without any subconnections */
    if (pcx->isPrimary () && !pcx->hasSubconnections())
      continue;

    if (sz > 0) {
      for (int arr = 0; arr < sz; arr++) {
	act_connection *imp = cx->getsubconn (arr, sz);
	act_connection *imp2;
	imp2 = imp->getsubconn (i, ux->getNumPorts());
	if (!imp2->vx) {
	  imp2->vx = pvx;
	}
	Assert (imp2->vx == pvx, "Hmm.");
	_import_conn_rec (imp, imp2, pcx, ux);
      }
    }
    else {
      act_connection *imp = cx->getsubconn (i, ux->getNumPorts());
      if (!imp->vx) {
	imp->vx = pvx;
      }
      Assert (imp->vx == pvx, "Hmm...");
      _import_conn_rec (cx, imp, pcx, ux);
    }
  }
}

ValueIdx *ActId::rootVx (Scope *s)
{
  return rawValueIdx (s);
}

ValueIdx *ActId::rawValueIdx (Scope *s)
{
  ValueIdx *vx;
  act_connection *cx;
  const char *topname;
  int is_ns;
  vx = NULL;
  
  Assert (s->isExpanded(), "ActId::FindCanonical called on unexpanded scope");

  is_ns = isNamespace();
  if (is_ns) {
    ActNamespace *curns = getNamespace();
    s = curns->CurScope();
    topname = Rest()->getName();
  }
  else {
    topname = getName();
  }

  while (s && !vx) {
    vx = s->LookupVal (topname);
    s = s->Parent ();
  }

  if (!vx) {
    act_error_ctxt (stderr);
    fprintf (stderr, " Id: ");
    Print (stderr);
    fprintf (stderr, "\nNot found in scope!\n");
    exit (1);
  }

  if (!vx->init) {
    vx->init = 1;
    cx = new act_connection (NULL);
    vx->u.obj.c = cx;
    cx->vx = vx;
    vx->u.obj.name = topname;

    /* now if vx is a user-defined type, check if it has any internal
       port connections. If so, add them here */
    if (TypeFactory::isUserType (vx->t)) {
      UserDef *ux = dynamic_cast<UserDef *>(vx->t->BaseType());
      Assert (ux, "Hmm");
#if 0      
      printf ("== Import from: %s\n", ux->getName());
#endif      
      _import_connections (cx, ux, vx->t->arrayInfo());
#if 0      
      printf ("== End import\n");
#endif      
    }
  }
  return vx;
}  


act_connection *ActId::myConnection (Scope *s)
{
  ValueIdx *vx = rawValueIdx (s);
  ActId *me;

  if (!vx) {
    return NULL;
  }

  if (isNamespace()) {
    me = Rest();
  }
  else {
    me = this;
  }

  if (me->Rest() && me->Rest()->Rest()) {
    return NULL;
  }
  if (!me->Rest()) {
    if (me->arrayInfo()) {
      return NULL;
    }
    else {
      return vx->u.obj.c;
    }
  }

  if (!vx->u.obj.c->hasSubconnections()) {
    return NULL;
  }

  UserDef *u = dynamic_cast <UserDef *> (vx->t->BaseType());
  if (!u) {
    return NULL;
  }

  int idx = u->FindPort (me->Rest()->getName());
  if (idx <= 0) {
    return NULL;
  }
  idx--;

  act_connection *tmp;
  tmp = vx->u.obj.c->getsubconn (idx, idx+1 /* should not need this param! */);

  if (!me->Rest()->arrayInfo()) {
    return tmp;
  }
  else {
    InstType *xt = u->getPortType (idx);

    if (!xt->arrayInfo()) return NULL;
    idx = xt->arrayInfo()->Offset (me->Rest()->arrayInfo());
    if (idx == -1) {
      return NULL;
    }
    return tmp->getsubconn (idx, idx+1);
  }
}

static void print_id (act_connection *c)
{
  list_t *stk = list_new ();
  ValueIdx *vx;

  while (c) {
    stack_push (stk, c);
    if (c->vx) {
      c = c->parent;
    }
    else if (c->parent->vx) {
      c = c->parent->parent;
    }
    else {
      Assert (c->parent->parent->vx, "What?");
      c = c->parent->parent->parent;
    }
    
  }

  while (!stack_isempty (stk)) {
    c = (act_connection *) stack_pop (stk);
    if (c->vx) {
      vx = c->vx;
      printf ("%s", vx->u.obj.name);
    }
    else if (c->parent->vx) {
      vx = c->parent->vx;
      if (vx->t->arrayInfo()) {
	Array *tmp;
	tmp = vx->t->arrayInfo()->unOffset (c->myoffset());
	printf ("%s", vx->u.obj.name);
	tmp->Print (stdout);
	delete tmp;
      }
      else {
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vx->t->BaseType());
	Assert (ux, "what?");
	printf ("%s.%s", vx->u.obj.name, ux->getPortName (c->myoffset()));
      }
    }
    else {
      vx = c->parent->parent->vx;
      Assert (vx, "What?");
      
      Array *tmp;
      tmp = vx->t->arrayInfo()->unOffset (c->parent->myoffset());
      UserDef *ux;
      ux = dynamic_cast<UserDef *> (vx->t->BaseType());
      Assert (ux, "what?");

      printf ("%s", vx->u.obj.name);
      tmp->Print (stdout);
      printf (".%s", ux->getPortName (c->myoffset()));

      delete tmp;
    }
    if (vx->global) {
      printf ("(g)");
    }
    if (!stack_isempty (stk)) {
      printf (".");
    }
  }
  
  list_free (stk);

#if 0
  printf ("<rev: ");

  while (c) {
    if (c->vx) {
      printf (".");
      if (c->vx->global) {
	char *s = c->vx->t->getNamespace()->Name();
	printf ("%s%s(g)", s, c->vx->u.obj.name);
	FREE (s);
      }
      else {
	UserDef *ux;
	printf ("{t:");
	ux = c->vx->t->getUserDef();
	if (ux) {
	  printf ("#%s#", ux->getName());
	}
	c->vx->t->Print (stdout);
	printf ("}%s", c->vx->u.obj.name);
      }
    }
    else {
      InstType *it;
      if (c->parent->vx) {
	/* one level: either x[] or x.y */
	it = c->parent->vx->t;
	if (it->arrayInfo()) {
	  printf ("[i:%d]", offset (c->parent->a, c));
	}
	else {
	  UserDef *ux = dynamic_cast<UserDef *>(it->BaseType());
	  Assert (ux, "What?");
	  printf (".%s", ux->getPortName (offset (c->parent->a, c)));
	}
      }
      else if (c->parent->parent->vx) {
	UserDef *ux;
	it = c->parent->parent->vx->t;
	/* x[].y */
	Assert (it->arrayInfo(), "What?");
	ux = dynamic_cast<UserDef *>(it->BaseType());
	Assert (ux, "What?");
	printf ("[i:%d]", offset (c->parent->parent->a, c->parent));
	printf (".%s", ux->getPortName(offset (c->parent->a, c)));
      }
      else {
	Assert (0, "What?");
      }
    }
    c = c->parent;
  }
  printf (">");
#endif  
}

    
/*
  Return canonical connection slot for identifier in scope.
  If it is a subrange identifier, it will be the array id rather than
  a reference to the subrange.
*/
act_connection *ActId::Canonical (Scope *s)
{
  ValueIdx *vx;
  act_connection *cx;
  ActId *id;
  ActId *idrest;
  
  Assert (s->isExpanded(), "ActId::Canonical called on unexpanded scope");

  //#define DEBUG_CONNECTIONS

#ifdef DEBUG_CONNECTIONS
  static int level = 0;
  printf ("===%d Canonical: ", level);
  Print (stdout);
  printf (" [scope=%s]\n", s->getName());
  level++;
#endif

  vx = rawValueIdx(s);
  cx = vx->u.obj.c;

#ifdef DEBUG_CONNECTIONS
  printf ("Type-vx: ");
  vx->t->Print (stdout);
  printf ("\n");
#endif  

  cx = cx->primary();
  
  vx = cx->getvx();

#ifdef DEBUG_CONNECTIONS
  printf ("Type-vx-update: ");
  vx->t->Print (stdout);
  printf ("\n");
#endif  
  

#ifdef DEBUG_CONNECTIONS
  printf (" --> conn-list: ");
  print_id (cx);
  {
    act_connection *tx;
    tx = cx;
    while (tx->next != cx) {
      tx = tx->next;
      printf ("=");
      print_id (tx);
    }
  }
  printf ("\n");
#endif  

  /* now cx could be something else; but it has the same type has the
     id's core name 
          i.e. for foo[x].bar...  or foo.bar...
               the type of vx/cx is the same as "foo"

	  foo = q[5];

           foo.bar --> foo's cx would be q[5]
        so really it is q[5].bar

            foo became q[5]
  */
  if (isNamespace()) {
    id = this->Rest();
    idrest = id->Rest();
  }
  else {
    id = this;
    idrest = id->Rest();
  }

  if (TypeFactory::isProcessType (vx->t) && vx->t->getIfaceType()) {
    UserDef *ux = dynamic_cast<UserDef *>(vx->t->BaseType());
    Assert (ux, "What?");
    
    /* extract the real type */
    InstType *itmp = vx->t->getIfaceType();
    Assert (itmp, "What?");
    Process *proc = dynamic_cast<Process *> (ux);
      Assert (proc, "What?");
      list_t *map = proc->findMap (itmp);
      if (!map) {
	fatal_error ("Missing interface `%s' from process `%s'?",
		     itmp->BaseType()->getName(), proc->getName());
      }
      listitem_t *li;
#if 0      
      printf ("ID rest: ");
      id->Rest()->Print(stdout);
      printf ("\n");
#endif      
	
      for (li = list_first (map); li; li = list_next (li)) {
	char *from = (char *)list_value (li);
	char *to = (char *)list_value (list_next(li));
	if (strcmp (id->Rest()->getName(), from) == 0) {
	  ActId *nid = new ActId (to);
	  if (id->Rest()->arrayInfo()) {
	    nid->setArray (id->Rest()->arrayInfo());
	  }
	  /* XXX: ID CACHING */
	  nid->Append (id->Rest()->Rest());
	  idrest = nid;
	  break;
	}
	li = list_next (li);
      }
      if (!li) {
	fatal_error ("Map for interface `%s' doesn't contain `%s'",
		     itmp->BaseType()->getName(), id->Rest()->getName());
      }
#if 0      
      printf ("NEW ID rest: ");
      idrest->Print(stdout);
      printf ("\n");
#endif      
    }
  
#if 0
  /*-- 
    Since we now pull in subtype connections through ports, this
    code is redundant.
  --*/
  ActId *topf = NULL;
  
  if (idrest) {
    /* now check to see if id->Reset() has a *DIFFERENT* 
       canonical name in the subtype! 

       1. It is a global! In that case, abort this loop and return
       the connection idx for the global.

       2. It is the same, no change. Continue as below.
       
       3. It is a different name. By construction, the new name must
       be a port.
       
       Replace id->Rest with the new id!
    */

    act_connection *cxrest;
    ValueIdx *vxrest;
    UserDef *ux;

    /* find canonical connection of rest in the scope for the inst
       type */

    ux = dynamic_cast<UserDef *>(vx->t->BaseType());
    Assert (ux, "What?");

    
    cxrest = idrest->Canonical (ux->CurScope());

#if 0
    printf ("------\n");
    printf ("Original name: ");
    idrest->Print (stdout);
    printf ("\nUnique name: ");
    print_id (cxrest);
    {
      act_connection *tx;
      tx = cxrest;
      while (tx->next != cxrest) {
	tx = tx->next;
	printf ("=");
	print_id (tx);
      }
    }
    printf("\n");
    dump_conn (cxrest);
    printf ("------\n");
#endif

    /* YYY: convert cxrest into an ID! */
    vxrest = cxrest->vx;
    if (!vxrest) {
      vxrest = cxrest->parent->vx;
      if (!vxrest) {
	vxrest = cxrest->parent->parent->vx;
      }
    }
    Assert (vxrest, "What?");
    if (vxrest->global) {
      /* we're done! */
#ifdef DEBUG_CONNECTIONS
      level--;
      printf (" -->%d returned-early: ", level);
      print_id (cxrest);
      printf ("\n");
#endif      
      return cxrest;
    }

    /* not global, so re-write the port to be the canonical port name.
       Note that this *will* be canonical at all levels. 
    */

    /* compute new idrest! */
    list_t *stk = list_new ();

    while (cxrest) {
      stack_push (stk, cxrest);
      if (cxrest->vx) {
	cxrest = cxrest->parent;
      }
      else if (cxrest->parent->vx) {
	cxrest = cxrest->parent->parent;
      }
      else {
	Assert (cxrest->parent->parent->vx, "What?");
	cxrest = cxrest->parent->parent->parent;
      }
    }
    ActId *fresh;

    fresh = NULL;
    /* replace idrest! */
    
    while (!stack_isempty (stk)) {
      cxrest = (act_connection *) stack_pop (stk);
      if (cxrest->vx) {
	vxrest = cxrest->vx;
	if (!fresh) {
	  fresh = new ActId (vxrest->u.obj.name);
	  topf = fresh;
	}
	else {
	  fresh->Append (new ActId (vxrest->u.obj.name));
	  fresh = fresh->Rest();
	}
      }
      else if (cxrest->parent->vx) {
	vxrest = cxrest->parent->vx;
	if (vxrest->t->arrayInfo()) {
	  Array *tmp;
	  tmp = vxrest->t->arrayInfo()->unOffset (cxrest->myoffset());
	  if (!fresh) {
	    fresh = new ActId (vxrest->u.obj.name, tmp);
	    topf = fresh;
	  }
	  else {
	    fresh->Append (new ActId (vxrest->u.obj.name, tmp));
	    fresh = fresh->Rest();
	  }
	}
	else {
	  UserDef *ux;

	  ux = dynamic_cast<UserDef *> (vxrest->t->BaseType());
	  Assert (ux, "???");
	  
	  if (!fresh) {
	    fresh = new ActId (vxrest->u.obj.name);
	    topf = fresh;
	  }
	  else {
	    fresh->Append (new ActId (vxrest->u.obj.name));
	    fresh = fresh->Rest();
	  }
	}
      }
      else {
	vxrest = cxrest->parent->parent->vx;
	Assert (vxrest, "???");

	Array *tmp;
	tmp = vxrest->t->arrayInfo()->unOffset (cxrest->parent->myoffset());
	UserDef *ux;
	ux = dynamic_cast<UserDef *> (vxrest->t->BaseType());
	Assert (ux, "what?");

	if (!fresh) {
	  fresh = new ActId (vxrest->u.obj.name, tmp);
	  topf = fresh;
	}
	else {
	  fresh->Append (new ActId (vxrest->u.obj.name, tmp));
	  fresh = fresh->Rest();
	  fresh->Append (new ActId (ux->getPortName (cxrest->myoffset())));
	}
      }
    }
    list_free (stk);
  }
#endif
  
#ifdef DEBUG_CONNECTIONS
  printf ("canonical: ");
  id->Print (stdout);
  printf (" [rest=");
  idrest->Print (stdout);
  printf ("]");
#if 0
  printf (" [new=");
  topf->Print (stdout);
  printf ("] ");
  fflush (stdout);
#endif  
  printf ("\n");
#endif

#if 0
  idrest = topf;
#endif  
  
  do {
    /* vx is the value 
       cx is the object
    */
    if (id->arrayInfo() && id->arrayInfo()->isDeref()) {
      /* find array slot, make vx the value, cx the connection id for
	 the canonical value */

      int idx = vx->t->arrayInfo()->Offset (id->arrayInfo());
      Assert (idx != -1, "This should have been caught earlier");

      cx = cx->getsubconn(idx, vx->t->arrayInfo()->size());
      cx = cx->primary();

      /* find the value slot for the canonical name */
      if (cx->vx) {
	vx = cx->vx;
      }
      else if (cx->parent->vx) {
	/* array */
	vx = cx->parent->vx;
      }
      else {
	/* array and subtype */
	vx = cx->parent->parent->vx;
      }
      Assert (vx, "What?");
    }
    
    if (idrest) {
      int portid;
      UserDef *ux;
      /* find port id */

      ux = dynamic_cast<UserDef *>(vx->t->BaseType());
#if 0      
      if (cx->vx == vx) {
	/* raw connection, basetype is fine */
      }
      else {
	if (id->arrayInfo()) {
	  if (cx->parent->vx == vx) {
	    /* cx is just array, basetype is fine */
	  }
	  else {
	    /* foo[].bar; arrays are first, so bar is the offset */
	    ux = dynamic_cast <UserDef *>
	      (ux->getPortType (offset (cx->parent->a, cx))->BaseType());
	  }
	}
	else {
	  ux = dynamic_cast<UserDef *>
	    (ux->getPortType (offset (cx->parent->a, cx))->BaseType());
	}
      }
#endif      
      Assert (ux, "Should have been caught earlier!");
      
#if 0
      printf ("Type: %s, port %s\n", ux->getName(), idrest->getName());
#endif      
      
      portid = ux->FindPort (idrest->getName());
      Assert (portid > 0, "What?");

      portid--;

      cx = cx->getsubconn (portid, ux->getNumPorts());

      /* WWW: is this right?! */
      cx->vx = idrest->rawValueIdx (ux->CurScope());
      
      cx = cx->primary();
      
      if (cx->vx) {
	vx = cx->vx;
      }
      else if (cx->parent->vx) {
	vx = cx->parent->vx;
      }
      else {
	vx = cx->parent->parent->vx;
      }
      Assert (vx, "What?");
    }
    id = idrest;
    if (idrest) {
      // we are fine, because we re-computed
      // the canonical port at the top.
      idrest = idrest->Rest();
    }
  } while (id);

#if 0  
  if (topf) {
    delete topf;
  }
#endif  

#ifdef DEBUG_CONNECTIONS
  level--;
  printf (" -->%d returned: ", level);
  print_id (cx);
  printf ("\n");
  dump_conn (cx);
  printf ("\n");
#endif
  
  return cx;
}  


int ActId::isEqual (ActId *id)
{
  if (!id) return 0;
  if (id->name != name) return 0;
  if (id->a && !a) return 0;
  if (!id->a && a) return 0;
  if (a) {
    if (!a->isEqual (id->a, 1)) return 0;
  }
  if (id->next && !next) return 0;
  if (!id->next && next) return 0;
  if (next) {
    return next->isEqual (id->next);
  }
  return 1;
}
  

ActId *ActId::Tail ()
{
  ActId *ret = this;
  while (ret->Rest()) {
    ret = ret->Rest();
  }
  return ret;
}

int ActId::isDynamicDeref ()
{
  int ret = 0;
  ActId *pr = this;
  ActId *id = this;
  if (id->arrayInfo()) {
    if (id->arrayInfo()->isDynamicDeref()) {
      ret = 1;
    }
  }
  id = id->Rest();
  while (id) {
    if (id->arrayInfo()) {
      if (id->arrayInfo()->isDynamicDeref()) {
	fprintf (stderr, "In examining ID: ");
	pr->Print (stderr);
	fprintf (stderr, "\n");
	fatal_error ("Only simple (first-level) dynamic derefs are supported");
      }
    }
    id = id->Rest();
  }
  return ret;
}

act_connection *ActId::rootCanonical (Scope *s)
{
  act_connection *c = Canonical (s);
  
  do {
    while (c->parent) {
      c = c->parent;
    }
    c = c->primary();
  } while (c->parent);

  return c;
}


UserDef *ActId::isFragmented (Scope *s)
{
  InstType *it;
  ActId *tmp;
  UserDef *u;
  if (!Rest()) return 0;

  tmp = this;

  if (isNamespace()) {
    s = getNamespace()->CurScope();
    tmp = Rest();
  }
  it = s->FullLookup (tmp->getName());
  
  Assert (it, "What?");
  while (tmp->Rest()) {
    UserDef *u = dynamic_cast<UserDef *> (it->BaseType());
    if (TypeFactory::isChanType (it)) {
      Assert (u, "Hi");
      return u;
    }
    if (TypeFactory::isDataType (it)) {
      Assert (u, "Hi");
      return u;
    }
    tmp = tmp->Rest();
    Assert (u, "What?");
    it = u->Lookup (tmp);
    Assert (it, "What?");
  }
  return NULL;
}

UserDef *ActId::canFragment (Scope *s)
{
  InstType *it;
  it = s->FullLookup (this, NULL);
  return dynamic_cast<UserDef *> (it->BaseType());
}

ActId *ActId::unFragment (Scope *s)
{
  InstType *it;
  ActId *tmp, *ret, *cur, *rtmp;
  
  if (!Rest()) return NULL;

  tmp = this;

  if (isNamespace()) {
    s = getNamespace()->CurScope();
    tmp = Rest();
  }
  it = s->FullLookup (tmp->getName());
  Assert (it, "What?");
  while (tmp->Rest()) {
    if (TypeFactory::isChanType (it)) {
      break;
    }
    if (TypeFactory::isDataType (it)) {
      break;
    }
    tmp = tmp->Rest();
    UserDef *u = dynamic_cast<UserDef *> (it->BaseType());
    Assert (u, "What?");
    it = u->Lookup (tmp);
    Assert (it, "What?");
  }
  if (!tmp->Rest()) {
    return NULL;
  }

  cur = this;
  ret = new ActId (string_cache (cur->getName()), cur->arrayInfo() ?
		   cur->arrayInfo()->Clone() : NULL);
  rtmp = ret;
  while (cur != tmp) {
    cur = cur->Rest();
    rtmp->next = new ActId (string_cache (cur->getName()), cur->arrayInfo() ?
			    cur->arrayInfo()->Clone() : NULL);
    rtmp = rtmp->next;
  }
  return ret;
}

ActId *ActId::parseId (const char *s)
{
  ActId *ret;
  
  if (!s) return NULL;
  
  char *t = Strdup (s);

  ret = ActId::parseId (t);

  FREE (t);

  return ret;
}

ActId *ActId::parseId (const char *s, const char delim1, const char arrayL,
		       const char arrayR, const char delim2)
{
  ActId *ret;
  
  if (!s) return NULL;
  
  char *t = Strdup (s);

  ret = ActId::parseId (t, delim1, arrayL, arrayR, delim2);

  FREE (t);

  return ret;
}

ActId *ActId::parseId (char *s, const char delim1, const char arrayL,
		       const char arrayR, const char delim2)
{
  int nextdot;
  int arrayinfo;
  int len;
  int which_delim = 1;

  ActId *ret = NULL;
  ActId *tmp;
  ActId *tail;

  if (!s) return NULL;

  do {
    nextdot = -1;
    
    for (int i=0; s[i]; i++) {
      if (s[i] == delim1) {
	which_delim = 1;
	nextdot = i;
	break;
      }
      else if (s[i] == delim2) {
	which_delim = 2;
	nextdot = i;
	break;
      }
    }
    if (nextdot != -1) {
      s[nextdot] = '\0';
    }
    if (nextdot != -1) {
      len = nextdot;
    }
    else {
      len = strlen (s);
    }

    arrayinfo = -1;
    
    for (int i=0; i < len; i++) {
      if (s[i] == arrayL) {
	arrayinfo = i;
	s[i] = '\0';
	break;
      }
    }

    tmp = new ActId (s, NULL);

    if (arrayinfo != -1) {
      int i;
      Array *ar;
      s[arrayinfo] = arrayL;

      ar = NULL;

      do {
	i = arrayinfo+1;
	while (i < len && isdigit (s[i])) {
	  i++;
	}
	if (i == len || s[i] != arrayR) {
	  /* parse error */
	  if (ret) {
	    delete ret;
	  }
	  if (ar) {
	    delete ar;
	  }
	  delete tmp;
	  if (nextdot != -1) {
	    if (which_delim == 1) {
	      s[nextdot] = delim1;
	    }
	    else {
	      s[nextdot] = delim2;
	    }
	  }
	  return NULL;
	}
	if (!ar) {
	  ar = new Array (atoi (s+arrayinfo+1));
	}
	else {
	  ar->Concat (new Array (atoi(s+arrayinfo+1)));
	}
	arrayinfo = i + 1;
      } while (s[arrayinfo] == arrayL);

      if (s[arrayinfo] != '\0') {
	if (ar) { delete ar; }
	if (ret) { delete ret; }
	delete tmp;
	if (nextdot != -1) {
	  if (which_delim == 1) {
	    s[nextdot] = delim1;
	  }
	  else {
	    s[nextdot] = delim2;
	  }
	}
	return NULL;
      }
      tmp->setArray (ar);
    }

    if (!ret) {
      ret = tmp;
    }
    else {
      tail->Append (tmp);
    }
    tail = tmp;

      
    if (nextdot != -1) {
      if (which_delim == 1) {
	s[nextdot] = delim1;
      }
      else {
	s[nextdot] = delim2;
      }
      s = s + nextdot + 1;
    }
  } while (nextdot != -1);

  return ret;
}  


ActId *ActId::parseId (char *s)
{
  return parseId (s, '.', '[', ']', '.');
}  

ActId *ActId::nonProcSuffix (Process *p, Process **ret)
{
  InstType *it;
  ActId *itmp = this;
  ActId *prev = NULL;
  Process *prev_proc = NULL;
  Scope *cursc;

  if (p) {
    cursc = p->CurScope();
  }
  else {
    cursc = NULL;
  }

  if (isNamespace()) {
    cursc = getNamespace()->CurScope();
    itmp = itmp->Rest();
  }
  
  while (itmp) {
    it = cursc->FullLookup (itmp->getName());
    Assert (it, "pre-cursor function should have checked for this!");
    if (!TypeFactory::isProcessType (it)) {
      if (prev_proc == NULL) {
	*ret = p;
	return itmp;
      }
      break;
    }
    prev_proc = p;
    prev = itmp;
    p = dynamic_cast<Process *> (it->BaseType());
    Assert (p, "Hmm");
    cursc = p->CurScope();
    itmp = itmp->Rest();
  }
  *ret = prev_proc;
  return prev;
}


int ActId::validateDeref (Scope *sc)
{
  ValueIdx *vx;
  int is_ns = isNamespace();

  if (is_ns) {
    vx = getNamespace()->CurScope()->FullLookupVal (Rest()->getName());
  }
  else {
    vx = sc->FullLookupVal (getName());
  }
  
  if (!vx) return 0;
  
  /* -- check all the array indices! -- */
  ValueIdx *ux = vx;
  ActId *rid = this;
  if (is_ns) {
    rid = rid->Rest();
  }
  while (ux) {
    int aoff;
    if (ux->t->arrayInfo()) {
      if (!rid->arrayInfo() || !rid->arrayInfo()->isDeref()) {
	/* error */
	return 0;
      }
      aoff = ux->t->arrayInfo()->Offset (rid->arrayInfo());
    }
    else {
      if (rid->arrayInfo()) {
	return 0;
      }
      aoff = 0;
    }
    if (aoff == -1) {
      return 0;
    }

    rid = rid->Rest();
    
    UserDef *user = dynamic_cast<UserDef *> (ux->t->BaseType());
    if (user) {
      sc = user->CurScope();
      if (rid) {
	ux = sc->LookupVal (rid->getName());
      }
      else {
	return 1;
      }
    }
    else {
      ux = NULL;
    }
    if (rid && !ux) {
      return 0;
    }
  }
  return 1;
}


ActId *ActId::stripArray ()
{
  ActId *tmp, *ret;

  ret = Clone();

  tmp = ret;
  while (tmp->Rest()) {
    tmp = tmp->Rest();
  }
  Array *x = tmp->arrayInfo();
  if (x) {
    delete x;
  }
  tmp->setArray (NULL);

  return ret;
}

  
/*------------------------------------------------------------------------
 *
 *  ActId::isNamespace --
 *
 *   Returns 1 if this is a namespace prefix, 0 otherwise
 *
 *------------------------------------------------------------------------
 */
int
ActId::isNamespace ()
{
  const char *s = getName();
  int i = 0;

  while (s[i] && s[i] != ':') {
    i++;
  }
  if (s[i] == ':') {
    Assert (Rest(), "Namespace prefix in ID must have an actual ID name");
    Assert (!a, "Namespace prefix should not have an array");
    return 1;
  }
  return 0;
}


/*------------------------------------------------------------------------
 *
 *  ActId::getNamespace --
 *
 *   Return the namespace for the namespace global ID
 *
 *------------------------------------------------------------------------
 */
ActNamespace *
ActId::getNamespace ()
{
  if (!isNamespace()) {
    return NULL;
  }
  
  ActNamespace *curns = ActNamespace::Global();
  char *tmpbuf = Strdup (getName());
  int i, j;

  if (tmpbuf[0] == ':' && tmpbuf[1] == ':') {
    i = 2;
  }
  else {
    i = 0;
  }

  while (1) {
    j = i;
    while (tmpbuf[i] && tmpbuf[i] != ':') {
      i++;
    }
    if (tmpbuf[i] == ':') {
      Assert (tmpbuf[i+1] == ':', "What?");
      tmpbuf[i] = '\0';
      curns = curns->findNS (tmpbuf+j);
      if (!curns) {
	act_error_ctxt (stderr);
	fatal_error ("ActId::getNamespace(): namespace `%s' not found (`%s'?",
		     getName(), tmpbuf+j);
      }
      i += 2;
    }
    else {
      FREE (tmpbuf);
      return curns;
    }
  }
  FREE (tmpbuf);
  return NULL;
}


/*------------------------------------------------------------------------
 *
 *  Return 1 if there is a non-local aspect to this ID, 0 otherwise.
 *
 *------------------------------------------------------------------------
 */
int ActId::isNonLocal (Scope *s)
{
  if (isDynamicDeref ()) {
    /* dynamic derefs are always local */
    return 0;
  }
  act_connection *c = rootCanonical (s);
  if (c->isglobal()) {
    return 1;
  }
  UserDef *ux = s->getUserDef();
  if (ux && !s->isFunction() && (ux->FindPort (c->getvx()->getName()) > 0)) {
    return 1;
  }
  return 0;
}
