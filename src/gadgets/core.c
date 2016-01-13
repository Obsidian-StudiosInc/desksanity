#include "e_mod_main.h"
#include "gadget.h"

#define SNAP_DISTANCE 5

#define IS_HORIZ(orient) \
  ((orient) == Z_GADGET_SITE_ORIENT_HORIZONTAL)

#define IS_VERT(orient) \
  ((orient) == Z_GADGET_SITE_ORIENT_VERTICAL)

#define ZGS_GET(obj) \
   Z_Gadget_Site *zgs; \
   zgs = evas_object_data_get((obj), "__z_gadget_site"); \
   if (!zgs) abort()

typedef struct Z_Gadget_Config Z_Gadget_Config;

typedef struct Z_Gadget_Site
{
   Evas_Object *layout;
   Evas_Object *events;
   Z_Gadget_Site_Gravity gravity;
   Z_Gadget_Site_Orient orient;
   Z_Gadget_Site_Anchor anchor;
   Z_Gadget_Style_Cb style_cb;
   Eina_List *gadgets;
   Eina_List *fixed_gadgets;
   int cur_size;

   Z_Gadget_Config *action;
   Ecore_Event_Handler *move_handler;
   Ecore_Event_Handler *mouse_up_handler;
} Z_Gadget_Site;


/* refcount? */
struct Z_Gadget_Config
{
   E_Object *e_obj_inherit; //list?
   int id;
   Evas_Object *display; //list?
   Evas_Object *gadget; //list?
   struct
   {
      Evas_Object *obj;
      int minw, minh;
      Eina_Stringshare *name;
   } style;
   Eina_Stringshare *type;
   Z_Gadget_Configure_Cb configure;
   Z_Gadget_Configure_Cb wizard;
   Evas_Object *cfg_object;
   Z_Gadget_Site *site;
   E_Menu *menu;

   double x, y; //fixed % positioning
   Evas_Point offset; //offset from mouse down
   Z_Gadget_Config *over; //gadget is animating over another gadget during drag
   Eina_Bool modifying : 1;
};

static Eina_Hash *gadget_types;

static Eina_List *sites;

static E_Action *move_act;
static E_Action *configure_act;
static E_Action *menu_act;

static Z_Gadget_Config *
_gadget_at_xy(Z_Gadget_Site *zgs, int x, int y, Z_Gadget_Config *exclude)
{
   Eina_List *l;
   Z_Gadget_Config *zgc, *saved = NULL;
   Evas_Object *win;
   int wx, wy;

   win = e_win_evas_object_win_get(zgs->layout);
   evas_object_geometry_get(win, &wx, &wy, NULL, NULL);
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     {
        int ox, oy, ow, oh;

        if (!zgc->gadget) continue;

        evas_object_geometry_get(zgc->display, &ox, &oy, &ow, &oh);
        if (E_INSIDE(x, y, ox + wx, oy + wy, ow, oh))
          {
             if (zgc == exclude) saved = zgc;
             else return zgc;
          }
     }
   if (saved) return saved;
   EINA_LIST_FOREACH(zgs->fixed_gadgets, l, zgc)
     {
        int ox, oy, ow, oh;

        if (!zgc->gadget) continue;

        evas_object_geometry_get(zgc->display, &ox, &oy, &ow, &oh);
        if (E_INSIDE(x, y, ox + wx, oy + wy, ow, oh))
          {
             if (zgc == exclude) saved = zgc;
             else return zgc;
          }
     }
   if (saved) return saved;
   return NULL;
}

static void
_gravity_apply(Evas_Object *ly, Z_Gadget_Site_Gravity gravity)
{
   double ax = 0.5, ay = 0.5;

   switch (gravity)
     {
      case Z_GADGET_SITE_GRAVITY_LEFT:
        ax = 0;
        break;
      case Z_GADGET_SITE_GRAVITY_RIGHT:
        ax = 1;
        break;
      default: break;
     }
   switch (gravity)
     {
      case Z_GADGET_SITE_GRAVITY_TOP:
        ay = 0;
        break;
      case Z_GADGET_SITE_GRAVITY_BOTTOM:
        ay = 1;
        break;
      default: break;
     }
   elm_box_align_set(ly, ax, ay);
}

static void
_gadget_reparent(Z_Gadget_Site *zgs, Z_Gadget_Config *zgc)
{
   switch (zgs->gravity)
     {
      case Z_GADGET_SITE_GRAVITY_NONE:
        /* fake */
        break;
      case Z_GADGET_SITE_GRAVITY_LEFT:
      case Z_GADGET_SITE_GRAVITY_TOP:
        elm_box_pack_end(zgs->layout, zgc->display);
        break;
      default:
        /* right aligned: pack on left */
        elm_box_pack_start(zgs->layout, zgc->display);
     }
}

static void
_gadget_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Z_Gadget_Config *zgc = data;

   if (!e_object_is_del(zgc->e_obj_inherit))
     e_object_del(zgc->e_obj_inherit);
}

static void
_gadget_object_free(E_Object *eobj)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;

   g = e_object_data_get(eobj);
   zgc = evas_object_data_get(g, "__z_gadget");
   evas_object_smart_callback_call(zgc->site->layout, "gadget_removed", zgc->gadget);
   E_FREE_FUNC(zgc->gadget, evas_object_del);
   evas_object_del(zgc->cfg_object);
   evas_object_del(zgc->style.obj);
   E_FREE(zgc->e_obj_inherit);
}

static void
_gadget_object_create(Z_Gadget_Config *zgc)
{
   Z_Gadget_Create_Cb cb;
   Evas_Object *g;

   cb = eina_hash_find(gadget_types, zgc->type);
   if (!cb) return; //can't create yet

   /* if id is < 0, gadget creates dummy config for demo use
    * if id is 0, gadget creates new config and returns id
    * otherwise, config of `id` is applied to created object
    */
   g = cb(zgc->site->layout, &zgc->id, zgc->site->orient);
   EINA_SAFETY_ON_NULL_RETURN(g);

   zgc->e_obj_inherit = E_OBJECT_ALLOC(E_Object, Z_GADGET_TYPE, _gadget_object_free);
   e_object_data_set(zgc->e_obj_inherit, g);
   zgc->gadget = zgc->display = g;
   evas_object_data_set(g, "__z_gadget", zgc);
   if (zgc->site->style_cb)
     zgc->site->style_cb(zgc->site->layout, NULL, g);

   evas_object_event_callback_add(g, EVAS_CALLBACK_DEL, _gadget_del, zgc);
   _gadget_reparent(zgc->site, zgc);
   evas_object_raise(zgc->site->events);

   evas_object_smart_callback_call(zgc->site->layout, "gadget_added", g);
   evas_object_smart_callback_call(zgc->site->layout, "gadget_gravity", g);

   evas_object_show(zgc->display);
}

static void
_site_gadget_resize(Evas_Object *g, int w, int h, Evas_Coord *ww, Evas_Coord *hh, Evas_Coord *ow, Evas_Coord *oh)
{
   Evas_Coord mnw, mnh, mxw, mxh;
   Z_Gadget_Config *zgc;
   Evas_Aspect_Control aspect;
   int ax, ay;

   zgc = evas_object_data_get(g, "__z_gadget");
   w -= zgc->style.minw;
   h -= zgc->style.minh;

   evas_object_size_hint_min_get(g, &mnw, &mnh);
   evas_object_size_hint_max_get(g, &mxw, &mxh);
   evas_object_size_hint_aspect_get(g, &aspect, &ax, &ay);

   if (IS_HORIZ(zgc->site->orient))
     {
        *ww = mnw, *hh = h;
        if (!(*ww)) *ww = *hh;
     }
   else if (IS_VERT(zgc->site->orient))
     {
        *hh = mnh, *ww = w;
        if (!(*hh)) *hh = *ww;
     }
   if (aspect && ax && ay)
     {
        switch (aspect)
          {
           case EVAS_ASPECT_CONTROL_HORIZONTAL:
             *hh = (*ww * ay / ax);
             break;
           case EVAS_ASPECT_CONTROL_VERTICAL:
             *ww = (*hh * ax / ay);
             break;
           default:
             if (IS_HORIZ(zgc->site->orient))
               *ww = (*hh * ax / ay);
             else
               *hh = (*ww * ay / ax);
          }
     }
   *ww += zgc->style.minw;
   *hh += zgc->style.minh;
   *ow = *ww, *oh = *hh;
   if ((!ax) && (!ay))
     {
        if ((mxw >= 0) && (mxw < *ow)) *ow = mxw;
        if ((mxh >= 0) && (mxh < *oh)) *oh = mxh;
     }

   evas_object_resize(zgc->display, *ow, *oh);
}

static void
_site_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int x, y, w, h;
   Z_Gadget_Site *zgs = data;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->events, x, y, w, h);
}

static void
_site_layout(Evas_Object *o, Evas_Object_Box_Data *priv EINA_UNUSED, void *data)
{
   Z_Gadget_Site *zgs = data;
   Evas_Coord x, y, w, h, xx, yy, px, py;
   Eina_List *l;
   double ax, ay;
   Z_Gadget_Config *zgc;

   evas_object_geometry_get(o, &x, &y, &w, &h);
   evas_object_geometry_set(zgs->events, x, y, w, h);

   evas_object_box_align_get(o, &ax, &ay);

   px = xx = x;
   py = yy = y;

   /* do layout for rest of gadgets now to avoid fixed gadgets */
   if (zgs->gravity % 2)//left/top
     {
        EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
          {
             Evas_Coord gx = xx, gy = yy;
             int ww, hh, ow, oh;

             _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
             if (IS_HORIZ(zgs->orient))
               gx += (Evas_Coord)(((double)(ww - ow)) * 0.5),
               gy += (h / 2) - (oh / 2);
             else if (IS_VERT(zgs->orient))
               gy += (Evas_Coord)(((double)(hh - oh)) * 0.5),
               gx += (w / 2) - (ow / 2);
             if (zgc->over)
               evas_object_stack_above(zgc->display, zgc->over->gadget);
             if (zgs->orient && ((zgc->x > -1) || (zgc->y > -1)))
               {
                  if (IS_HORIZ(zgs->orient))
                    evas_object_move(zgc->display, zgc->x * (double)w, gy);
                  else
                    evas_object_move(zgc->display, gx, zgc->y * (double)h);
               }
             else
               evas_object_move(zgc->display, gx, gy);
             if (IS_HORIZ(zgs->orient))
               xx += ow;
             else
               yy += oh;
          }
     }
   else if (zgs->gravity)
     {
        if (IS_HORIZ(zgs->orient))
          px += w;
        else
          py += h;
        xx = px, yy = py;

        EINA_LIST_REVERSE_FOREACH(zgs->gadgets, l, zgc)
          {
             Evas_Coord gx = xx, gy = yy;
             int ww, hh, ow, oh;

             _site_gadget_resize(zgc->gadget, w, h, &ww, &hh, &ow, &oh);
             if (IS_HORIZ(zgs->orient))
               gx -= (Evas_Coord)(((double)(ww - ow)) * 0.5) + ow,
               gy += (h / 2) - (oh / 2);
             else
               gy -= (Evas_Coord)(((double)(hh - oh)) * 0.5) + oh,
               gx += (w / 2) - (ow / 2);
             evas_object_move(zgc->display, gx, gy);
             if (IS_HORIZ(zgs->orient))
               xx -= ow;
             else
               yy -= oh;
          }
     }
   px = xx;
   py = yy;

   if (IS_HORIZ(zgs->orient))
     zgs->cur_size = abs((ax * w) - px) - x;
   else if (IS_VERT(zgs->orient))
     zgs->cur_size = abs((ay * h) - py) - y;

   evas_object_size_hint_min_set(zgs->layout,
     IS_HORIZ(zgs->orient) ? zgs->cur_size : w,
     IS_VERT(zgs->orient) ? zgs->cur_size : h);
   /* do layout for fixed position gadgets after */
   EINA_LIST_REVERSE_FOREACH(zgs->fixed_gadgets, l, zgc)
     {
        Evas_Coord gx = xx, gy = yy;
        int ww, hh, ow, oh;

        _site_gadget_resize(zgc->gadget, w - (px - x), h - (py - y), &ww, &hh, &ow, &oh);
        if (IS_HORIZ(zgc->site->orient))
          {
             gx = ((1 - ax) * xx) + (zgc->x * (w - zgs->cur_size));
             gx += (Evas_Coord)(((double)(ww - ow)) * 0.5 * -ax);
          }
        else
          {
             gy = ((1 - ay) * yy) + (zgc->y * (h - zgs->cur_size));
             gy += (Evas_Coord)(((double)(hh - oh)) * 0.5 * -ay);
          }
        if (zgs->gravity % 2)//left/top
          {
             if (gx < px) gx = px;
          }
        else
          {
             if (gx > px) gx = px;
          }

        if (zgs->gravity % 2)//left/top
          {
             if (gy < py) gy = py;
          }
        else
          {
             if (gy > py) gy = py;
          }

        evas_object_move(zgc->display, gx, gy);
        if (IS_HORIZ(zgs->orient))
          px = gx + (-ax * ow);
        else
          py = gy + (-ay * oh);
     }
}

static Eina_Bool
_gadget_mouse_move(Z_Gadget_Config *zgc, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   int wx, wy;//window coords
   int x, y, w, h;//site geom
   int mx, my;//mouse coords normalized for layout orientation
   int gw, gh;//"relative" region size
   int *rw, *rh;//"relative" region size aliasing
   int ox, oy, ow, oh;//gadget geom
   Z_Gadget_Config *z;
   Evas_Object *win;

   /* adjust window -> screen coords */
   win = e_win_evas_object_win_get(zgc->site->layout);
   evas_object_geometry_get(win, &wx, &wy, NULL, NULL);
   evas_object_geometry_get(zgc->site->layout, &x, &y, &w, &h);
   x += wx, y += wy;
   gw = w, gh = h;

   mx = ev->x;
   my = ev->y;

   evas_object_geometry_get(zgc->display, &ox, &oy, &ow, &oh);
   ox += wx, oy += wy;

   rw = &gw;
   rh = &gh;
   /* normalize constrained axis to get a valid coordinate */
   if (IS_HORIZ(zgc->site->orient))
     {
        my = y + 1;
        *rw = zgc->site->cur_size;
     }
   else
     {
        mx = x + 1;
        *rh = zgc->site->cur_size;
     }
#define OUT \
  fprintf(stderr, "OUT %d\n", __LINE__)
   if (E_INSIDE(mx, my, x, y, w, h))
     {
        /* dragging inside site */
        int sx = x, sy = y;
        double ax, ay;

        /* adjust contiguous site geometry for gravity */
        elm_box_align_get(zgc->site->layout, &ax, &ay);
        if (IS_HORIZ(zgc->site->orient))
          sx = x + ((w - zgc->site->cur_size) * ax);
        else
          sy = y + ((h - zgc->site->cur_size) * ay);
        if (E_INSIDE(mx, my, sx, sy, *rw + SNAP_DISTANCE, *rh + SNAP_DISTANCE))
          {
             /* dragging inside relative area */
             int ggx, ggy, ggw, ggh;
             Eina_List *l;
             Eina_Bool left = EINA_FALSE;//moving gadget is "left" of this gadget

             EINA_LIST_FOREACH(zgc->site->gadgets, l, z)
               {
                  if (z == zgc)
                    {
                       left = EINA_TRUE;
                       continue;
                    }
                  evas_object_geometry_get(z->gadget, &ggx, &ggy, &ggw, &ggh);
                  ggx += wx, ggy += wy;
                  if (E_INTERSECTS(ox, oy, ow, oh, ggx, ggy, ggw, ggh)) break;
               }

             if (z && (z != zgc))
               {
                  /* found a gadget that is not the current gadget */
                  int *pmx, *pggx, *pggw, *pwx, *px, *pw, *offx;
                  double *zx;

                  if (IS_HORIZ(zgc->site->orient))
                    pmx = &mx, pggx = &ggx, pggw = &ggw, pwx = &wx,
                    px = &x, pw = &w, zx = &zgc->x, offx = &zgc->offset.x;
                  else
                    pmx = &my, pggx = &ggy, pggw = &ggh, pwx = &wy,
                    px = &y, pw = &h, zx = &zgc->y, offx = &zgc->offset.y;
                  if (left)
                    {
                       if (*pmx >= *pggx + (*pggw / 2)) // more than halfway over
                         {
                            if (eina_list_data_get(l->next) != zgc)
                              {
                                 zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                                 zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                                 zgc->site->gadgets = eina_list_append_relative_list(zgc->site->gadgets, zgc, l);
                                 zgc->over = NULL;
                                 *zx = -1.0;
                                 OUT;
                              }
                         }
                       else // less
                         {
                            *zx = (double)(*pmx - *pwx - *px - *offx) / (double)*pw;
                            zgc->over = z;
                            OUT;
                         }
                    }
                  else
                    {
                       if (*pmx <= *pggx + (*pggw / 2)) // more than halfway over
                         {
                            if (eina_list_data_get(l->prev) != zgc)
                              {
                                 zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                                 zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                                 zgc->site->gadgets = eina_list_prepend_relative_list(zgc->site->gadgets, zgc, l);
                                 zgc->over = NULL;
                                 *zx = -1.0;
                                 OUT;
                              }
                         }
                       else // less
                         {
                            *zx = (double)(*pmx - *pwx - *px - *offx) / (double)*pw;
                            zgc->over = z;
                            OUT;
                         }
                    }
               }
             else if (E_INSIDE(mx, my, sx, sy, *rw, *rh))
               {
                  /* no found gadget: dragging over current gadget's area */
                  if (IS_HORIZ(zgc->site->orient))
                    {
                       /* clamp to site geometry */
                       if (mx - wx - x - zgc->offset.x > 0)
                         {
                            if (mx - wx - x - zgc->offset.x + ow <= w)
                              {zgc->x = (double)(mx - wx - x - zgc->offset.x) / w;OUT;}
                            else
                              {zgc->x = -1;OUT;}
                         }
                       else
                         {zgc->x = -1;OUT;}
                    }
                  else if (IS_VERT(zgc->site->orient))
                    {
                       /* clamp to site geometry */
                       if (my - wy - y - zgc->offset.y > 0) 
                         {
                            if (my - wy - y - zgc->offset.y + oh <= h)
                              {zgc->y = (double)(my - wy - y - zgc->offset.y) / h;OUT;}
                            else
                              {zgc->y = -1;OUT;}
                         }
                       else
                         {zgc->y = -1;OUT;}
                    }
                  zgc->over = NULL;
               }
             else
               {
                  if (eina_list_data_get(zgc->site->gadgets) == zgc)
                    {
                       /* first gadget in site dragging past itself:
                        * lock position
                        */
                       if (IS_HORIZ(zgc->site->orient))
                         zgc->x = -1;
                       else if (IS_VERT(zgc->site->orient))
                         zgc->y = -1;
                    }
                  else
                    {
                       zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                       zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                       zgc->site->gadgets = eina_list_append(zgc->site->gadgets, zgc);
                       if (IS_HORIZ(zgc->site->orient))
                         zgc->x = -1;
                       else if (IS_VERT(zgc->site->orient))
                         zgc->y = -1;
                    }
               }
          }
        else
          {
             /* dragging outside relative area */
             if (zgc->site->fixed_gadgets)
               {
                  Eina_List *l;
                  Z_Gadget_Config *zz;

                  EINA_LIST_FOREACH(zgc->site->fixed_gadgets, l, zz)
                    {
                       int zx, zy, zw, zh;

                       if ((zz = zgc) || (!zz->gadget)) continue;

                       /* FIXME: shortcut */
                       evas_object_geometry_get(zz->gadget, &zx, &zy, &zw, &zh);
                       if (E_INSIDE(mx, my, zx, zy, zw, zh))
                         {
                            zgc->over = zz;
                            break;
                         }
                    }
               }
             if (!((zgc->x > -1) || (zgc->y > -1)))
               {
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_append(zgc->site->fixed_gadgets, zgc);
               }
             if (IS_HORIZ(zgc->site->orient))
               zgc->x = (double)(mx - wx - zgc->offset.x - ((1 - ax) * zgc->site->cur_size)) / (double)w;
             else
               zgc->y = (double)(my - wy - zgc->offset.y - ((1 - ay) * zgc->site->cur_size)) / (double)h;
             OUT;
          }
     }
   else
     {
        /* dragging to edge of site */
        Eina_Bool left;
        double *fx;

        if (IS_HORIZ(zgc->site->orient))
          {
             fx = &zgc->x;
             left = mx <= x;
          }
        else
          {
             fx = &zgc->y;
             left = my <= y;
          }
        if (left)
          {
             if (zgc->site->gravity % 2) //left/top
               {
                  *fx = -1.0;
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                  zgc->site->gadgets = eina_list_prepend(zgc->site->gadgets, zgc);OUT;
               }
             else
               {
                  *fx = 0.0;
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_prepend(zgc->site->fixed_gadgets, zgc);OUT;
               }
          }
        else
          {
             if (zgc->site->gravity % 2) //left/top
               {
                  *fx = 1.0;
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_append(zgc->site->fixed_gadgets, zgc);OUT;
               }
             else
               {
                  *fx = -1.0;
                  zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
                  zgc->site->fixed_gadgets = eina_list_remove(zgc->site->fixed_gadgets, zgc);
                  zgc->site->gadgets = eina_list_append(zgc->site->gadgets, zgc);OUT;
               }
          }
     }
   elm_box_recalculate(zgc->site->layout);

   return ECORE_CALLBACK_RENEW;
}

static void
_gadget_act_modify_end(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;
   Eina_Bool recalc = EINA_FALSE;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__z_gadget");
   zgc->modifying = 0;
   if (zgc->over)
     {
      OUT;
        /* FIXME: animate */
        zgc->x = zgc->y = -1.0;
        zgc->over = NULL;
        recalc = 1;
     }
   if (recalc)
     elm_box_recalculate(zgc->site->layout);

   E_FREE_FUNC(zgc->site->move_handler, ecore_event_handler_del);
}

static void
_gadget_act_modify(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;

   if (obj->type != Z_GADGET_TYPE) return;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__z_gadget");
   zgc->modifying = 1;
   if (!zgc->site->move_handler)
     zgc->site->move_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE, (Ecore_Event_Handler_Cb)_gadget_mouse_move, zgc);
}

static void
_gadget_act_configure_object_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Z_Gadget_Config *zgc = data;

   zgc->cfg_object = NULL;
}

static void
_gadget_configure(Z_Gadget_Config *zgc)
{
   if (!zgc->configure) return;
   if (zgc->cfg_object)
     {
        evas_object_raise(zgc->cfg_object);
        evas_object_show(zgc->cfg_object);
        return;
     }
   zgc->cfg_object = zgc->configure(zgc->gadget);
   if (!zgc->cfg_object) return;
   evas_object_event_callback_add(zgc->cfg_object, EVAS_CALLBACK_DEL, _gadget_act_configure_object_del, zgc);
}

static void
_gadget_act_configure(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev EINA_UNUSED)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;

   if (obj->type != Z_GADGET_TYPE) return;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__z_gadget");
   _gadget_configure(zgc);
}

static void
_gadget_menu_remove(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   Z_Gadget_Config *zgc = data;

   zgc->site->gadgets = eina_list_remove(zgc->site->gadgets, zgc);
   evas_object_del(zgc->gadget);
}

static void
_gadget_menu_configure(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   _gadget_configure(data);
}

static void
_gadget_style_menu_item_del(void *mi)
{
   eina_stringshare_del(e_object_data_get(mi));
}

static void
_gadget_menu_style(void *data, E_Menu *m EINA_UNUSED, E_Menu_Item *mi)
{
   Z_Gadget_Config *zgc = data;
   Eina_Stringshare *style = e_object_data_get(E_OBJECT(mi));

   if (zgc->site->style_cb)
     zgc->site->style_cb(zgc->site->layout, style, zgc->gadget);
}

static void
_gadget_act_menu(E_Object *obj, const char *params EINA_UNUSED, E_Binding_Event_Mouse_Button *ev)
{
   Z_Gadget_Config *zgc;
   Evas_Object *g;
   E_Menu_Item *mi;
   E_Menu *subm;
   int x, y;

   if (obj->type != Z_GADGET_TYPE) return;

   g = e_object_data_get(obj);
   zgc = evas_object_data_get(g, "__z_gadget");

   zgc->menu = e_menu_new();
   evas_object_smart_callback_call(g, "gadget_menu", zgc->menu);
   if (zgc->configure)
     {
        mi = e_menu_item_new(zgc->menu);
        e_menu_item_label_set(mi, _("Settings"));
        e_util_menu_item_theme_icon_set(mi, "configure");
        e_menu_item_callback_set(mi, _gadget_menu_configure, zgc);
     }
   if (zgc->menu->items)
     {
        mi = e_menu_item_new(zgc->menu);
        e_menu_item_separator_set(mi, 1);
     }
   subm = e_menu_new();
   evas_object_smart_callback_call(zgc->site->layout, "gadget_style_menu", subm);
   if (e_object_data_get(E_OBJECT(subm)))
     {
        Eina_List *styles = e_object_data_get(E_OBJECT(subm));
        Eina_Stringshare *style;

        mi = e_menu_item_new(zgc->menu);
        e_menu_item_label_set(mi, _("Look"));
        e_util_menu_item_theme_icon_set(mi, "preferences-look");
        e_menu_item_submenu_set(mi, subm);
        e_object_unref(E_OBJECT(subm));

        EINA_LIST_FREE(styles, style)
          {
             char buf[1024];

             if (eina_streq(style, "base"))
               {
                  eina_stringshare_del(style);
                  continue;
               }
             mi = e_menu_item_new(subm);
             strncpy(buf, style, sizeof(buf) - 1);
             buf[0] = toupper(buf[0]);
             e_menu_item_label_set(mi, buf);
             snprintf(buf, sizeof(buf), "enlightenment/%s", style);
             e_util_menu_item_theme_icon_set(mi, buf);
             e_menu_item_radio_group_set(mi, 1);
             e_menu_item_radio_set(mi, 1);
             e_menu_item_toggle_set(mi, style == zgc->style.name);
             e_menu_item_disabled_set(mi, mi->toggle);
             e_object_data_set(E_OBJECT(mi), style);
             E_OBJECT_DEL_SET(mi, _gadget_style_menu_item_del);
             e_menu_item_callback_set(mi, _gadget_menu_style, zgc);
          }
     }
   else
     e_object_del(E_OBJECT(subm));


   mi = e_menu_item_new(zgc->menu);
   evas_object_smart_callback_call(zgc->site->layout, "gadget_owner_menu", mi);
   if (mi->label)
     {
        mi = e_menu_item_new(zgc->menu);
        e_menu_item_separator_set(mi, 1);
     }
   else
     e_object_del(E_OBJECT(mi));

   mi = e_menu_item_new(zgc->menu);
   e_menu_item_label_set(mi, _("Remove"));
   e_util_menu_item_theme_icon_set(mi, "list-remove");
   e_menu_item_callback_set(mi, _gadget_menu_remove, zgc);

   evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
   e_menu_activate_mouse(zgc->menu,
                         e_zone_current_get(),
                         x, y, 1, 1,
                         E_MENU_POP_DIRECTION_AUTO, ev->timestamp);
   evas_object_smart_callback_call(zgc->site->layout, "gadget_popup", zgc->menu->comp_object);
}

static Eina_Bool
_site_mouse_up(Z_Gadget_Site *zgs, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   if (e_bindings_mouse_up_ecore_event_handle(E_BINDING_CONTEXT_ANY, zgs->action->e_obj_inherit, ev))
     {
        evas_object_pointer_mode_set(zgs->events, EVAS_OBJECT_POINTER_MODE_NOGRAB);
        zgs->action = NULL;
        E_FREE_FUNC(zgs->mouse_up_handler, ecore_event_handler_del);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_site_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Z_Gadget_Site *zgs = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Z_Gadget_Config *zgc;
   E_Action *act;

   zgc = _gadget_at_xy(zgs, ev->output.x, ev->output.y, NULL);
   if (!zgc) return;
   act = e_bindings_mouse_down_evas_event_handle(E_BINDING_CONTEXT_ANY, zgc->e_obj_inherit, event_info);
   if (act && act->func.end_mouse)
     {
        int x, y;

        evas_object_pointer_mode_set(obj, EVAS_OBJECT_POINTER_MODE_NOGRAB_NO_REPEAT_UPDOWN);
        zgs->action = zgc;
        if (!zgs->mouse_up_handler)
          zgs->mouse_up_handler = ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP, (Ecore_Event_Handler_Cb)_site_mouse_up, zgs);


        evas_object_geometry_get(zgc->display, &x, &y, NULL, NULL);
        zgc->offset.x = ev->canvas.x - x;
        zgc->offset.y = ev->canvas.y - y;
     }
}

static void
_site_drop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Z_Gadget_Site *zgs = data, *drop;
   Eina_List *l;
   Z_Gadget_Config *zgc, *dzgc;
   int mx, my;
   int x, y, w, h;

   drop = evas_object_data_get(event_info, "__z_gadget_site");
   evas_pointer_canvas_xy_get(e_comp->evas, &mx, &my);
   evas_object_geometry_get(zgs->layout, &x, &y, &w, &h);
   if (!E_INSIDE(mx, my, x, y, w, h)) return;
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     {
        if (!zgc->display) continue;

        evas_object_geometry_get(zgc->display, &x, &y, &w, &h);
        if (E_INSIDE(mx, my, x, y, w, h)) break;
     }
   if (zgc)
     {
        Eina_Bool pre = EINA_FALSE;
        if (IS_HORIZ(zgs->orient))
          {
             if (mx <= x + (w / 2))
               pre = EINA_TRUE;
          }
        else if (IS_VERT(zgs->orient))
          {
             if (my <= y + (h / 2))
               pre = EINA_TRUE;
          }
        else {} //FIXME
        if (zgs->orient)
          {
             Eina_List *ll;

             if (pre)
               EINA_LIST_REVERSE_FOREACH(drop->gadgets, ll, dzgc)
                 {
                    evas_object_del(dzgc->gadget);
                    zgs->gadgets = eina_list_prepend_relative_list(zgs->gadgets, dzgc, l);
                    dzgc->site = zgs;
                    _gadget_object_create(dzgc);
                 }
             else
               EINA_LIST_REVERSE_FOREACH(drop->gadgets, ll, dzgc)
                 {
                    evas_object_del(dzgc->gadget);
                    zgs->gadgets = eina_list_append_relative_list(zgs->gadgets, dzgc, l);
                    dzgc->site = zgs;
                    _gadget_object_create(dzgc);
                 }
          }
     }
   else
     {
        if (zgs->orient)
          {
             if (mx >= x) //right of all exiting gadgets
               {
                  EINA_LIST_FOREACH(drop->gadgets, l, dzgc)
                    {
                       evas_object_del(dzgc->gadget);
                       zgs->gadgets = eina_list_append(zgs->gadgets, dzgc);
                       dzgc->site = zgs;
                       _gadget_object_create(dzgc);
                    }
               }
             else
               {
                  EINA_LIST_REVERSE_FOREACH(drop->gadgets, l, dzgc)
                    {
                       evas_object_del(dzgc->gadget);
                       zgs->gadgets = eina_list_prepend(zgs->gadgets, dzgc);
                       dzgc->site = zgs;
                       _gadget_object_create(dzgc);
                    }
               }
          }
     }
}

Z_API Evas_Object *
z_gadget_site_add(Evas_Object *parent, Z_Gadget_Site_Orient orient)
{
   Z_Gadget_Site *zgs;

   zgs = E_NEW(Z_Gadget_Site, 1);

   zgs->orient = orient;
   switch (orient)
     {
      case Z_GADGET_SITE_ORIENT_HORIZONTAL:
        zgs->gravity = Z_GADGET_SITE_GRAVITY_LEFT;
        break;
      case Z_GADGET_SITE_ORIENT_VERTICAL:
        zgs->gravity = Z_GADGET_SITE_GRAVITY_TOP;
        break;
      default: break;
     }

   zgs->layout = elm_box_add(parent);
   elm_box_horizontal_set(zgs->layout, orient == Z_GADGET_SITE_ORIENT_HORIZONTAL);
   _gravity_apply(zgs->layout, zgs->gravity);
   elm_box_layout_set(zgs->layout, _site_layout, zgs, NULL);

   zgs->events = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_event_callback_add(zgs->layout, EVAS_CALLBACK_MOVE, _site_move, zgs);
   evas_object_smart_callback_add(zgs->layout, "gadget_site_dropped", _site_drop, zgs);
   evas_object_pointer_mode_set(zgs->events, EVAS_OBJECT_POINTER_MODE_NOGRAB);
   evas_object_smart_member_add(zgs->events, zgs->layout);
   evas_object_color_set(zgs->events, 0, 0, 0, 0);
   evas_object_repeat_events_set(zgs->events, 1);
   evas_object_show(zgs->events);
   evas_object_event_callback_add(zgs->events, EVAS_CALLBACK_MOUSE_DOWN, _site_mouse_down, zgs);

   evas_object_data_set(zgs->layout, "__z_gadget_site", zgs);

   sites = eina_list_append(sites, zgs);

   if (!move_act)
     {
        move_act = e_action_add("gadget_move");
        e_action_predef_name_set(D_("Gadgets"), D_("Move gadget"), "gadget_move", NULL, NULL, 0);
        move_act->func.go_mouse = _gadget_act_modify;
        move_act->func.end_mouse = _gadget_act_modify_end;

        configure_act = e_action_add("gadget_configure");
        e_action_predef_name_set(D_("Gadgets"), D_("Configure gadget"), "gadget_configure", NULL, NULL, 0);
        configure_act->func.go_mouse = _gadget_act_configure;

        menu_act = e_action_add("gadget_menu");
        e_action_predef_name_set(D_("Gadgets"), D_("Gadget menu"), "gadget_menu", NULL, NULL, 0);
        menu_act->func.go_mouse = _gadget_act_menu;
     }

   return zgs->layout;
}

Z_API Z_Gadget_Site_Anchor
z_gadget_site_anchor_get(Evas_Object *obj)
{
   ZGS_GET(obj);

   return zgs->anchor;
}

Z_API void
z_gadget_site_owner_setup(Evas_Object *obj, Z_Gadget_Site_Anchor an, Z_Gadget_Style_Cb cb)
{
   ZGS_GET(obj);

   zgs->anchor = an;
   zgs->style_cb = cb;
   evas_object_smart_callback_call(obj, "gadget_anchor", NULL);
}

Z_API Z_Gadget_Site_Orient
z_gadget_site_orient_get(Evas_Object *obj)
{
   ZGS_GET(obj);
   return zgs->orient;
}

Z_API Z_Gadget_Site_Gravity
z_gadget_site_gravity_get(Evas_Object *obj)
{
   ZGS_GET(obj);
   return zgs->gravity;
}

Z_API void
z_gadget_site_gravity_set(Evas_Object *obj, Z_Gadget_Site_Gravity gravity)
{
   ZGS_GET(obj);
   zgs->gravity = gravity;
   evas_object_smart_need_recalculate_set(zgs->layout, 1);
}

Z_API Eina_List *
z_gadget_site_gadgets_list(Evas_Object *obj)
{
   Eina_List *l, *list = NULL;
   Z_Gadget_Config *zgc;

   ZGS_GET(obj);
   EINA_LIST_FOREACH(zgs->gadgets, l, zgc)
     if (zgc->display)
       list = eina_list_append(list, zgc->display);
   return list;
}

Z_API void
z_gadget_site_gadget_add(Evas_Object *obj, const char *type, Eina_Bool demo)
{
   Z_Gadget_Config *zgc;
   int id = 0;

   EINA_SAFETY_ON_NULL_RETURN(gadget_types);
   ZGS_GET(obj);
   demo = !!demo;
   id -= demo;

   zgc = E_NEW(Z_Gadget_Config, 1);
   zgc->id = id;
   zgc->type = eina_stringshare_add(type);
   zgc->x = -1;
   zgc->y = -1;
   zgc->site = zgs;
   _gadget_object_create(zgc);
   if (zgc->display)
     {
        zgc->site->gadgets = eina_list_append(zgc->site->gadgets, zgc);
        return;
     }
   eina_stringshare_del(zgc->type);
   free(zgc);
}

Z_API Evas_Object *
z_gadget_site_get(Evas_Object *g)
{
   Z_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);
   return zgc->site->layout;
}

Z_API void
z_gadget_configure_cb_set(Evas_Object *g, Z_Gadget_Configure_Cb cb)
{
   Z_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN(g);
   zgc = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN(zgc);
   zgc->configure = cb;
}

Z_API void
z_gadget_configure(Evas_Object *g)
{
   Z_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN(g);
   zgc = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN(zgc);
   _gadget_configure(zgc);
}

Z_API Eina_Bool
z_gadget_has_wizard(Evas_Object *g)
{
   Z_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, EINA_FALSE);
   zgc = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, EINA_FALSE);
   return !!zgc->wizard;
}

Z_API Eina_Stringshare *
z_gadget_type_get(Evas_Object *g)
{
   Z_Gadget_Config *zgc;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);
   return zgc->type;
}

Z_API void
z_gadget_type_add(const char *type, Z_Gadget_Create_Cb callback)
{
   if (!gadget_types) gadget_types = eina_hash_string_superfast_new(NULL);
   eina_hash_add(gadget_types, type, callback);
}

Z_API void
z_gadget_type_del(const char *type)
{
   Eina_List *l, *ll;
   Z_Gadget_Site *zgs;
   Z_Gadget_Config *zgc;
   char buf[1024];

   strncpy(buf, type, sizeof(buf));

   if (!gadget_types) return;

   EINA_LIST_FOREACH(sites, l, zgs)
     {
        EINA_LIST_FOREACH(zgs->gadgets, ll, zgc)
          if (eina_streq(buf, zgc->type))
            evas_object_del(zgc->gadget);
        EINA_LIST_FOREACH(zgs->fixed_gadgets, ll, zgc)
          if (eina_streq(buf, zgc->type))
            evas_object_del(zgc->gadget);
     }
}

Z_API Eina_Iterator *
z_gadget_type_iterator_get(void)
{
   return gadget_types ? eina_hash_iterator_key_new(gadget_types) : NULL;
}

Z_API Evas_Object *
z_gadget_util_layout_style_init(Evas_Object *g, Evas_Object *style)
{
   Z_Gadget_Config *zgc;
   Evas_Object *prev;
   const char *grp;

   EINA_SAFETY_ON_NULL_RETURN_VAL(g, NULL);
   zgc = evas_object_data_get(g, "__z_gadget");
   EINA_SAFETY_ON_NULL_RETURN_VAL(zgc, NULL);

   prev = zgc->style.obj;
   zgc->style.obj = style;
   if (style)
     {
        elm_layout_file_get(style, NULL, &grp);
        eina_stringshare_replace(&zgc->style.name, strrchr(grp, '/') + 1);
     }
   else
     eina_stringshare_replace(&zgc->style.name, NULL);
   zgc->display = style ?: zgc->gadget;
   E_FILL(zgc->display);
   E_EXPAND(zgc->display);
   if (zgc->site->gravity)
     {
        elm_box_pack_before(zgc->site->layout, zgc->display, prev ?: zgc->gadget);
        if (prev)
          elm_box_unpack(zgc->site->layout, prev);
     }
   evas_object_raise(zgc->site->events);
   if (!style) return prev;

   evas_object_data_set(style, "__z_gadget", zgc);

   elm_layout_sizing_eval(style);
   evas_object_smart_calculate(style);
   evas_object_size_hint_min_get(style, &zgc->style.minw, &zgc->style.minh);
   evas_object_show(style);
   return prev;
}
