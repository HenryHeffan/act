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
#include <stdio.h>
#include <string.h>
#include <act/passes.h>
#include <act/passes/netlist.h>
#include <act/passes/aflat.h>
#include <map>
#include <common/config.h>

void usage (char *s)
{
  fprintf (stderr, "Usage: %s [act-options] <file.act> <simfile>\n", s);
  exit (1);
}

void idprint (FILE *fp, ActId *id)
{
  char buf[10240];
  int i;
  if (id) {
    id->sPrint (buf, 10240);
  }
  else {
    buf[0] = '\0';
  }
  for (i=0; buf[i]; i++) {
    if (buf[i] == '.') {
      buf[i] = '/';
    }
  }
  fprintf (fp, "%s", buf);
}


void f (void *x, ActId *one, ActId *two)
{
  FILE *fp = (FILE *) x;
  fprintf (fp, "= ");
  idprint (fp, one);
  //one->Print (fp);
  fprintf (fp, " ");
  idprint (fp, two);
  //two->Print (fp);
  fprintf (fp, "\n");
}

static ActNetlistPass *netinfo = NULL;

static void _print_node (netlist_t *N, FILE *fp, ActId *prefix, node_t *n)
{
  if (n->v) {
    ActId *tmp = n->v->v->id->toid();
    if (!n->v->v->id->isglobal()) {
      idprint (fp, prefix);
      //prefix->Print (fp);
      //fprintf (fp, ".");
      fprintf (fp, "/");
    }
    //tmp->Print (fp);
    idprint (fp, tmp);
    delete tmp;
  }
  else {
    if (n == N->Vdd) {
      fprintf (fp, "Vdd");
    }
    else if (n == N->GND) {
      fprintf (fp, "GND");
    }
    else {
      if (prefix) {
	//prefix->Print (fp);
	//fprintf (fp, ".");
	idprint (fp, prefix);
	fprintf (fp, "/");
      }
      fprintf (fp, "n#%d", n->i);
    }
  }
}

void g (void *x, ActId *prefix, UserDef *u)
{
  FILE *fp;
  netlist_t *N;
  Process *p;

  p = dynamic_cast <Process *> (u);
  if (!p) return;

  N = netinfo->getNL (p);
  
  Assert (N, "Hmm");

  fp = (FILE *)x;

  /* now print out the netlist, with all names having the prefix
     specified */
  node_t *n;
  edge_t *e;
  listitem_t *li;
  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      if (e->visited) continue;
      /* p <gate> <src> <drain> l w */
      if (e->type == EDGE_NFET) {
	fprintf (fp, "n ");
      }
      else {
	fprintf (fp, "p ");
      }
      _print_node (N, fp, prefix, e->g);
      fprintf (fp, " ");
      _print_node (N, fp, prefix, e->a);
      fprintf (fp, " ");
      _print_node (N, fp, prefix, e->b);
      fprintf (fp, " %d %d\n", e->l/ActNetlistPass::getGridsPerLambda(),
	       e->w/ActNetlistPass::getGridsPerLambda());
      e->visited = 1;
    }
  }

  for (n = N->hd; n; n = n->next) {
    for (li = list_first (n->e); li; li = list_next (li)) {
      e = (edge_t *) list_value (li);
      e->visited = 0;
    }
  }
  
}

int main (int argc, char **argv)
{
  Act *a;
  char *file;

  Act::Init (&argc, &argv);

  if (argc != 3) {
    usage (argv[0]);
  }
  
  a = new Act (argv[1]);
  a->Expand ();

  /* generate netlist */
  netinfo = new ActNetlistPass (a);
  netinfo->run ();

  FILE *fps, *fpal;
  char buf[10240];

  sprintf (buf, "%s.sim", argv[2]);
  fps = fopen (buf, "w");
  if (!fps) {
    fatal_error ("Could not open file `%s' for writing", buf);
  }
  sprintf (buf, "%s.al", argv[2]);
  fpal = fopen (buf, "w");
  if (!fpal) {
    fatal_error ("Could not open file `%s' for writing", buf);
  }

  /* print as sim file 
     units: lambda in centimicrons
   */
  double units = 1.0e8*config_get_real ("net.lambda");
  if (units <= 0) {
    warning ("Technology lambda is less than 1 centimicron; setting to 1");
    units = 1;
  }
  if (units == (int) units) {
    fprintf (fps, "| units: %d tech: %s format: MIT\n", (int)units, config_get_string ("net.name"));
  } else {
    fprintf (fps, "| units: %g tech: %s format: MIT\n", units, config_get_string ("net.name"));
  }
  
  ActApplyPass *app = new ActApplyPass (a);

  app->setCookie (fps);
  app->setInstFn (g);
  app->run ();
  g(fps, NULL, NULL);
  fclose (fps);

  app->setCookie (fpal);
  app->setInstFn (NULL);
  app->setConnPairFn (f);
  app->run ();
  fprintf (fpal, "= Vdd Vdd!\n");
  fprintf (fpal, "= GND GND!\n");
  fclose (fpal);

  return 0;
}
