/**
 * @file
 *
 * @internal This file is part of netcdf-4, a netCDF-like interface
 * for HDF5, or a HDF5 backend for netCDF, depending on your point of
 * view.
 *
 * This file handles the nc4 user-defined type functions
 * (i.e. compound and opaque types).
 *
 * Copyright 2005, University Corporation for Atmospheric Research. See
 * the COPYRIGHT file for copying and redistribution conditions.
 *
 * @author Ed Hartnett
 */
#include "nc4internal.h"
#include "nc4dispatch.h"

#define NUM_ATOMIC_TYPES 13 /**< Number of netCDF atomic types. */

/** @internal Names of atomic types. */
char atomic_name[NUM_ATOMIC_TYPES][NC_MAX_NAME + 1] = {"none", "byte", "char", 
						       "short", "int", "float", 
						       "double", "ubyte",
						       "ushort", "uint",
						       "int64", "uint64", "string"};

/* The sizes of types may vary from platform to platform, but within
 * netCDF files, type sizes are fixed. */
#define NC_CHAR_LEN sizeof(char)      /**< @internal Size of char. */
#define NC_STRING_LEN sizeof(char *)  /**< @internal Size of char *. */
#define NC_BYTE_LEN 1     /**< @internal Size of byte. */
#define NC_SHORT_LEN 2    /**< @internal Size of short. */
#define NC_INT_LEN 4      /**< @internal Size of int. */
#define NC_FLOAT_LEN 4    /**< @internal Size of float. */
#define NC_DOUBLE_LEN 8   /**< @internal Size of double. */
#define NC_INT64_LEN 8    /**< @internal Size of int64. */

/**
 * @internal Determine if two types are equal.
 *
 * @param ncid1 First file/group ID.
 * @param typeid1 First type ID.
 * @param ncid2 Second file/group ID.
 * @param typeid2 Second type ID.
 * @param equalp Pointer that will get 1 if the two types are equal.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @return ::NC_EINVAL Invalid type.
 * @author Ed Hartnett
 */
extern int
NC4_inq_type_equal(int ncid1, nc_type typeid1, int ncid2, 
		  nc_type typeid2, int *equalp)
{
   NC_GRP_INFO_T *grpone, *grptwo;
   NC_TYPE_INFO_T *type1, *type2;
   int retval;
   
   LOG((2, "nc_inq_type_equal: ncid1 0x%x typeid1 %d ncid2 0x%x typeid2 %d", 
	ncid1, typeid1, ncid2, typeid2));

   /* Check input. */
   if(equalp == NULL) return NC_NOERR;

   if (typeid1 <= NC_NAT || typeid2 <= NC_NAT)
      return NC_EINVAL;

   /* If one is atomic, and the other user-defined, the types are not
    * equal. */
   if ((typeid1 <= NC_STRING && typeid2 > NC_STRING) ||
       (typeid2 <= NC_STRING && typeid1 > NC_STRING))
   {
      if (equalp) *equalp = 0;
      return NC_NOERR;
   }

   /* If both are atomic types, the answer is easy. */
   if (typeid1 <= NUM_ATOMIC_TYPES)
   {
      if (equalp)
      {
	 if (typeid1 == typeid2)
	    *equalp = 1;
	 else
	    *equalp = 0;
      }	    
      return NC_NOERR;
   }

   /* Not atomic types - so find type1 and type2 information. */
   if ((retval = nc4_find_nc4_grp(ncid1, &grpone)))
      return retval;
   if (!(type1 = nc4_rec_find_nc_type(grpone->nc4_info->root_grp, 
				      typeid1)))
      return NC_EBADTYPE;
   if ((retval = nc4_find_nc4_grp(ncid2, &grptwo)))
      return retval;
   if (!(type2 = nc4_rec_find_nc_type(grptwo->nc4_info->root_grp, 
				      typeid2)))
      return NC_EBADTYPE;

   /* Are the two types equal? */
   if (equalp)
   {
      if ((retval = H5Tequal(type1->native_hdf_typeid, type2->native_hdf_typeid)) < 0)
         return NC_EHDFERR;
      *equalp = 1 ? retval : 0;
   }
   
   return NC_NOERR;
}

/**
 * @internal Get the id of a type from the name. 
 *
 * @param ncid File and group ID.
 * @param name Name of type.
 * @param typeidp Pointer that will get the type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_ENOMEM Out of memory.
 * @return ::NC_EINVAL Bad size.
 * @return ::NC_ENOTNC4 User types in netCDF-4 files only.
 * @return ::NC_EBADTYPE Type not found.
 * @author Ed Hartnett
 */
extern int
NC4_inq_typeid(int ncid, const char *name, nc_type *typeidp)
{
   NC_GRP_INFO_T *grp;
   NC_GRP_INFO_T *grptwo;
   NC_HDF5_FILE_INFO_T *h5;
   NC_TYPE_INFO_T *type = NULL;
   char *norm_name;
   int i, retval;

   /* Handle atomic types. */
   for (i = 0; i < NUM_ATOMIC_TYPES; i++)
      if (!strcmp(name, atomic_name[i]))
      {
	 if (typeidp)
	    *typeidp = i;
	 return NC_NOERR;
      }

   /* Find info for this file and group, and set pointer to each. */
   if ((retval = nc4_find_grp_h5(ncid, &grp, &h5)))
      return retval;
   assert(h5 && grp);

   /* If the first char is a /, this is a fully-qualified
    * name. Otherwise, this had better be a local name (i.e. no / in
    * the middle). */
   if (name[0] != '/' && strstr(name, "/"))
      return NC_EINVAL;

   /* Normalize name. */
   if (!(norm_name = (char*)malloc(strlen(name) + 1)))
      return NC_ENOMEM;
   if ((retval = nc4_normalize_name(name, norm_name))) {
     free(norm_name);
     return retval;
   }
   /* Is the type in this group? If not, search parents. */
   for (grptwo = grp; grptwo; grptwo = grptwo->parent)
      for (type = grptwo->type; type; type = type->l.next)
	 if (!strcmp(norm_name, type->name))
	 {
	    if (typeidp)
	       *typeidp = type->nc_typeid;
	    break;
	 }

   /* Still didn't find type? Search file recursively, starting at the
    * root group. */
   if (!type)
      if ((type = nc4_rec_find_named_type(grp->nc4_info->root_grp, norm_name)))
	 if (typeidp)
	    *typeidp = type->nc_typeid;

   free(norm_name);

   /* OK, I give up already! */
   if (!type)
      return NC_EBADTYPE;
   
   return NC_NOERR;
}

/**
 * @internal Find all user-defined types for a location. This finds
 * all user-defined types in a group.
 *
 * @param ncid File and group ID.
 * @param ntypes Pointer that gets the number of user-defined
 * types. Ignored if NULL
 * @param typeids Array that gets the typeids. Ignored if NULL.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @author Ed Hartnett
 */
int 
NC4_inq_typeids(int ncid, int *ntypes, int *typeids)
{
   NC_GRP_INFO_T *grp;
   NC_HDF5_FILE_INFO_T *h5;
   NC_TYPE_INFO_T *type;
   int num = 0;
   int retval;

   LOG((2, "nc_inq_typeids: ncid 0x%x", ncid));

   /* Find info for this file and group, and set pointer to each. */
   if ((retval = nc4_find_grp_h5(ncid, &grp, &h5)))
      return retval;
   assert(h5 && grp);

   /* Count types. */
   if (grp->type)
      for (type = grp->type; type; type = type->l.next)
      {
	 if (typeids)
	    typeids[num] = type->nc_typeid;
	 num++;
      }

   /* Give the count to the user. */
   if (ntypes)
      *ntypes = num;

   return NC_NOERR;
}

/**
 * @internal This internal function adds a new user defined type to
 * the metadata of a group of an open file.
 *
 * @param ncid File and group ID.
 * @param size Size in bytes of new type.
 * @param name Name of new type.
 * @param base_typeid Base type ID.
 * @param type_class NC_VLEN, NC_ENUM, or NC_STRING
 * @param typeidp Pointer that gets new type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_ENOTNC4 User types in netCDF-4 files only.
 * @return ::NC_EINVAL Bad size.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
static int
add_user_type(int ncid, size_t size, const char *name, nc_type base_typeid,
	      nc_type type_class, nc_type *typeidp)
{
   NC_HDF5_FILE_INFO_T *h5;
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   char norm_name[NC_MAX_NAME + 1];
   int retval;

   /* Check and normalize the name. */
   if ((retval = nc4_check_name(name, norm_name)))
      return retval;

   LOG((2, "%s: ncid 0x%x size %d name %s base_typeid %d ", 
	__FUNCTION__, ncid, size, norm_name, base_typeid));

   /* Find group metadata. */
   if ((retval = nc4_find_grp_h5(ncid, &grp, &h5)))
      return retval;
   assert(h5 && grp);

   /* Turn on define mode if it is not on. */
   if (!(h5->cmode & NC_INDEF))
      if ((retval = NC4_redef(ncid)))
	 return retval;

   /* No size is provided for vlens or enums, get it from the base type. */
   if (type_class == NC_VLEN || type_class == NC_ENUM)
   {
      if ((retval = nc4_get_typelen_mem(grp->nc4_info, base_typeid, 0, 
					&size)))
	 return retval;
   }
   else if (size <= 0)
      return NC_EINVAL;

   /* Check that this name is not in use as a var, grp, or type. */
   if ((retval = nc4_check_dup_name(grp, norm_name)))
      return retval;
   
   /* Add to our list of types. */
   if ((retval = nc4_type_list_add(grp, size, norm_name, &type)))
      return retval;

   /* Remember info about this type. */
   type->nc_type_class = type_class;
   if (type_class == NC_VLEN)
      type->u.v.base_nc_typeid = base_typeid;
   else if (type_class == NC_ENUM)
      type->u.e.base_nc_typeid = base_typeid;
   
   /* Return the typeid to the user. */
   if (typeidp)
      *typeidp = type->nc_typeid;

   return NC_NOERR;
}

/**
 * @internal Get the name and size of a type. For strings, 1 is
 * returned. For VLEN the base type len is returned.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param name Gets the name of the type.
 * @param size Gets the size of one element of the type in bytes.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @author Ed Hartnett
*/
int
NC4_inq_type(int ncid, nc_type typeid1, char *name, size_t *size)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   int atomic_size[NUM_ATOMIC_TYPES] = {0, NC_BYTE_LEN, NC_CHAR_LEN, NC_SHORT_LEN, 
					NC_INT_LEN, NC_FLOAT_LEN, NC_DOUBLE_LEN, 
					NC_BYTE_LEN, NC_SHORT_LEN, NC_INT_LEN, NC_INT64_LEN, 
					NC_INT64_LEN, NC_STRING_LEN};
					
   int retval;
   
   LOG((2, "nc_inq_type: ncid 0x%x typeid %d", ncid, typeid1));

   /* If this is an atomic type, the answer is easy. */
   if (typeid1 < NUM_ATOMIC_TYPES)
   {
      if (name)
	strcpy(name, atomic_name[typeid1]);
      if (size)
	*size = atomic_size[typeid1];
      return NC_NOERR;
   }

   /* Not an atomic type - so find group. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;
   
   /* Find this type. */
   if (!(type = nc4_rec_find_nc_type(grp->nc4_info->root_grp, typeid1)))
      return NC_EBADTYPE;

   if (name)
      strcpy(name, type->name);
   
   if (size)
   {
      if (type->nc_type_class == NC_VLEN)
	 *size = sizeof(nc_vlen_t);
      else if (type->nc_type_class == NC_STRING)
	 *size = 1;
      else
	 *size = type->size;
   }
   
   return NC_NOERR;
}

/**
 * @internal Create a compound type.
 *
 * @param ncid File and group ID.
 * @param size Gets size in bytes of one element of type.
 * @param name Name of the type.
 * @param typeidp Gets the type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
int
NC4_def_compound(int ncid, size_t size, const char *name, nc_type *typeidp)
{
   return add_user_type(ncid, size, name, 0, NC_COMPOUND, typeidp);
}

/**
 * @internal Insert a named field into a compound type.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param name Name of the type.
 * @param offset Offset of field.
 * @param field_typeid Field type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
int
NC4_insert_compound(int ncid, nc_type typeid1, const char *name, size_t offset, 
		   nc_type field_typeid)
{
   return nc_insert_array_compound(ncid, typeid1, name, offset, 
				   field_typeid, 0, NULL);
}

/**
 * @internal Insert a named array into a compound type.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param name Name of the array field.
 * @param offset Offset in bytes.
 * @param field_typeid Type of field.
 * @param ndims Number of dims for field.
 * @param dim_sizesp Array of dim sizes.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
extern int
NC4_insert_array_compound(int ncid, int typeid1, const char *name, 
			 size_t offset, nc_type field_typeid,
			 int ndims, const int *dim_sizesp)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   char norm_name[NC_MAX_NAME + 1];
   int retval;

   LOG((2, "nc_insert_array_compound: ncid 0x%x, typeid %d name %s "
	"offset %d field_typeid %d ndims %d", ncid, typeid1, 
	name, offset, field_typeid, ndims));

   /* Check and normalize the name. */
   if ((retval = nc4_check_name(name, norm_name)))
      return retval;

   /* Find file metadata. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;

   /* Find type metadata. */
   if ((retval = nc4_find_type(grp->nc4_info, typeid1, &type)))
      return retval;

   /* Did the user give us a good compound type typeid? */
   if (!type || type->nc_type_class != NC_COMPOUND)
      return NC_EBADTYPE;

   /* If this type has already been written to the file, you can't
    * change it. */
   if (type->committed)
      return NC_ETYPDEFINED;

   /* Insert new field into this type's list of fields. */
   if ((retval = nc4_field_list_add(&type->u.c.field, type->u.c.num_fields, 
				    norm_name, offset, 0, 0, field_typeid,
				    ndims, dim_sizesp)))
      return retval;
   type->u.c.num_fields++;
   
   return NC_NOERR;
}

/**
 * @internal Find info about any user defined type.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param name Gets name of the type.
 * @param size Gets size in bytes of one element of type.
 * @param base_nc_typep Gets the base nc_type.
 * @param nfieldsp Gets the number of fields.
 * @param classp Gets the type class (NC_COMPOUND, NC_ENUM, NC_VLEN).
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @author Ed Hartnett
*/
int
NC4_inq_user_type(int ncid, nc_type typeid1, char *name, size_t *size, 
		 nc_type *base_nc_typep, size_t *nfieldsp, int *classp)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   int retval;
   
   LOG((2, "nc_inq_user_type: ncid 0x%x typeid %d", ncid, typeid1));

   /* Find group metadata. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;
   
   /* Find this type. */
   if (!(type = nc4_rec_find_nc_type(grp->nc4_info->root_grp, typeid1)))
      return NC_EBADTYPE;

   /* Count the number of fields. */
   if (nfieldsp)
   {
      if (type->nc_type_class == NC_COMPOUND)
         *nfieldsp = type->u.c.num_fields;
      else if (type->nc_type_class == NC_ENUM)
	 *nfieldsp = type->u.e.num_members;
      else
	 *nfieldsp = 0;
   }

   /* Fill in size and name info, if desired. */
   if (size)
   {
      if (type->nc_type_class == NC_VLEN)
	 *size = sizeof(nc_vlen_t);
      else if (type->nc_type_class == NC_STRING)
	 *size = 1;
      else
	 *size = type->size;
   }
   if (name)
      strcpy(name, type->name);

   /* VLENS and ENUMs have a base type - that is, they type they are
    * arrays of or enums of. */
   if (base_nc_typep)
   {
      if (type->nc_type_class == NC_ENUM)
         *base_nc_typep = type->u.e.base_nc_typeid;
      else if (type->nc_type_class == NC_VLEN)
         *base_nc_typep = type->u.v.base_nc_typeid;
      else
         *base_nc_typep = NC_NAT;
   }

   /* If the user wants it, tell whether this is a compound, opaque,
    * vlen, enum, or string class of type. */
   if (classp)
      *classp = type->nc_type_class;

   return NC_NOERR;
}

/**
 * @internal Given the ncid, typeid and fieldid, get info about the
 * field.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param fieldid Field ID.
 * @param name Gets name of field.
 * @param offsetp Gets offset of field.
 * @param field_typeidp Gets field type ID.
 * @param ndimsp Gets number of dims for this field.
 * @param dim_sizesp Gets the dim sizes for this field.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @author Ed Hartnett
*/
int
NC4_inq_compound_field(int ncid, nc_type typeid1, int fieldid, char *name, 
		      size_t *offsetp, nc_type *field_typeidp, int *ndimsp, 
		      int *dim_sizesp)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   NC_FIELD_INFO_T *field;
   int d, retval;
   
   /* Find file metadata. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;
   
   /* Find this type. */
   if (!(type = nc4_rec_find_nc_type(grp->nc4_info->root_grp, typeid1)))
      return NC_EBADTYPE;

   /* Find the field. */
   for (field = type->u.c.field; field; field = field->l.next)
      if (field->fieldid == fieldid)
      {
	 if (name)
	    strcpy(name, field->name);
	 if (offsetp)
	    *offsetp = field->offset;
	 if (field_typeidp)
	    *field_typeidp = field->nc_typeid;
	 if (ndimsp)
	    *ndimsp = field->ndims;
	 if (dim_sizesp)
	    for (d = 0; d < field->ndims; d++)
	       dim_sizesp[d] = field->dim_size[d];
	 return NC_NOERR;
      }

   return NC_EBADFIELD;
}

/**
 * @internal Find a netcdf-4 file. THis will return an error if it
 * finds a netcdf-3 file, or a netcdf-4 file with strict nc3 rules.
 *
 * @param ncid File and group ID.
 * @param nc Pointer to pointer that gets NC struct for file.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_ESTRICTNC3 File uses classic model.
 * @author Ed Hartnett
*/
static int
find_nc4_file(int ncid, NC **nc)
{
   NC_HDF5_FILE_INFO_T* h5;
   
   /* Find file metadata. */
   if (!((*nc) = nc4_find_nc_file(ncid, &h5)))
      return NC_EBADID;
   assert(h5);
      
   if (h5->cmode & NC_CLASSIC_MODEL)
      return NC_ESTRICTNC3;

   return NC_NOERR;
}

/**
 * @internal Given the typeid and the name, get the fieldid.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param name Name of field.
 * @param fieldidp Pointer that gets new field ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @return ::NC_EBADFIELD Field not found.
 * @author Ed Hartnett
*/
int
NC4_inq_compound_fieldindex(int ncid, nc_type typeid1, const char *name, int *fieldidp)
{
   NC *nc;
   NC_TYPE_INFO_T *type;
   NC_FIELD_INFO_T *field;
   char norm_name[NC_MAX_NAME + 1];
   int retval;

   LOG((2, "nc_inq_compound_fieldindex: ncid 0x%x typeid %d name %s",
	ncid, typeid1, name));

   /* Find file metadata. */
   if ((retval = find_nc4_file(ncid, &nc)))
      return retval;

   /* Find the type. */
   if ((retval = nc4_find_type(NC4_DATA(nc), typeid1, &type)))
      return retval;

   /* Did the user give us a good compound type typeid? */
   if (!type || type->nc_type_class != NC_COMPOUND)
      return NC_EBADTYPE;

   /* Normalize name. */
   if ((retval = nc4_normalize_name(name, norm_name)))
      return retval;

   /* Find the field with this name. */
   for (field = type->u.c.field; field; field = field->l.next)
      if (!strcmp(field->name, norm_name))
	 break;

   if (!field)
      return NC_EBADFIELD;

   if (fieldidp)
      *fieldidp = field->fieldid;
   return NC_NOERR;
}


/* Opaque type. */

/**
 * @internal Create an opaque type. Provide a size and a name.
 *
 * @param ncid File and group ID.
 * @param datum_size Size in bytes of a datum.
 * @param name Name of new vlen type.
 * @param typeidp Pointer that gets new type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
int
NC4_def_opaque(int ncid, size_t datum_size, const char *name, 
	      nc_type *typeidp)
{
   return add_user_type(ncid, datum_size, name, 0, NC_OPAQUE, typeidp);
}


/**
 * @internal Define a variable length type.
 *
 * @param ncid File and group ID.
 * @param name Name of new vlen type.
 * @param base_typeid Base type of vlen.
 * @param typeidp Pointer that gets new type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
int
NC4_def_vlen(int ncid, const char *name, nc_type base_typeid, 
	    nc_type *typeidp)
{
   return add_user_type(ncid, 0, name, base_typeid, NC_VLEN, typeidp);
}

/**
 * @internal Create an enum type. Provide a base type and a name. At
 * the moment only ints are accepted as base types.
 *
 * @param ncid File and group ID.
 * @param base_typeid Base type of vlen.
 * @param name Name of new vlen type.
 * @param typeidp Pointer that gets new type ID.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EMAXNAME Name is too long.
 * @return ::NC_EBADNAME Name breaks netCDF name rules.
 * @author Ed Hartnett
*/
int
NC4_def_enum(int ncid, nc_type base_typeid, const char *name, 
	    nc_type *typeidp)
{
   return add_user_type(ncid, 0, name, base_typeid, NC_ENUM, typeidp);
}


/**
 * @internal Get enum name from enum value. Name size will be <=
 * NC_MAX_NAME.
 *
 * @param ncid File and group ID.
 * @param xtype Type ID.
 * @param value Value of enum.
 * @param identifier Gets the identifier for this enum value.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @return ::NC_EINVAL Invalid type data.
 * @author Ed Hartnett
*/
int
NC4_inq_enum_ident(int ncid, nc_type xtype, long long value, char *identifier)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   NC_ENUM_MEMBER_INFO_T *enum_member;
   long long ll_val;
   int i;
   int retval;

   LOG((3, "nc_inq_enum_ident: xtype %d value %d\n", xtype, value));
   
   /* Find group metadata. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;
   
   /* Find this type. */
   if (!(type = nc4_rec_find_nc_type(grp->nc4_info->root_grp, xtype)))
      return NC_EBADTYPE;
   
   /* Complain if they are confused about the type. */
   if (type->nc_type_class != NC_ENUM)
      return NC_EBADTYPE;
   
   /* Move to the desired enum member in the list. */
   enum_member = type->u.e.enum_member;
   for (i = 0; i < type->u.e.num_members; i++)
   {
      switch (type->u.e.base_nc_typeid)
      {
	 case NC_BYTE:
	    ll_val = *(char *)enum_member->value;
	    break;
	 case NC_UBYTE:
	    ll_val = *(unsigned char *)enum_member->value;
	    break;
	 case NC_SHORT:
	    ll_val = *(short *)enum_member->value;
	    break;
	 case NC_USHORT:
	    ll_val = *(unsigned short *)enum_member->value;
	    break;
	 case NC_INT:
	    ll_val = *(int *)enum_member->value;
	    break;
	 case NC_UINT:
	    ll_val = *(unsigned int *)enum_member->value;
	    break;
	 case NC_INT64:
	 case NC_UINT64:
	    ll_val = *(long long *)enum_member->value;
	    break;
	 default:
	    return NC_EINVAL;
      }
      LOG((4, "ll_val=%d", ll_val));
      if (ll_val == value)
      {
	 if (identifier)
	    strcpy(identifier, enum_member->name);
	 break;
      }
      else
	 enum_member = enum_member->l.next;
   }

   /* If we didn't find it, life sucks for us. :-( */
   if (i == type->u.e.num_members)
      return NC_EINVAL;

   return NC_NOERR;
}

/**
 * @internal Get information about an enum member: an identifier and
 * value. Identifier size will be <= NC_MAX_NAME.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param idx Enum member index.
 * @param identifier Gets the identifier.
 * @param value Gets the enum value.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @return ::NC_EINVAL Bad idx.
 * @author Ed Hartnett
*/
int
NC4_inq_enum_member(int ncid, nc_type typeid1, int idx, char *identifier, 
		   void *value)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   NC_ENUM_MEMBER_INFO_T *enum_member;
   int i;
   int retval;
   
   LOG((2, "nc_inq_enum_member: ncid 0x%x typeid %d", ncid, typeid1));

   /* Find group metadata. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;
   
   /* Find this type. */
   if (!(type = nc4_rec_find_nc_type(grp->nc4_info->root_grp, typeid1)))
      return NC_EBADTYPE;
   
   /* Complain if they are confused about the type. */
   if (type->nc_type_class != NC_ENUM)
      return NC_EBADTYPE;
   
   /* Check index. */
   if (idx >= type->u.e.num_members)
      return NC_EINVAL;
   
   /* Move to the desired enum member in the list. */
   enum_member = type->u.e.enum_member;
   for (i = 0; i < idx; i++)
      enum_member = enum_member->l.next;

   /* Give the people what they want. */
   if (identifier)
      strcpy(identifier, enum_member->name);
   if (value)
      memcpy(value, enum_member->value, type->size);

   return NC_NOERR;
}

/**
 * @internal Insert a identifier value into an enum type. The value
 * must fit within the size of the enum type, the identifier size must
 * be <= NC_MAX_NAME.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param identifier Name of this enum value.
 * @param value Value of enum.
 *
 * @return ::NC_NOERR No error.
 * @return ::NC_EBADID Bad ncid.
 * @return ::NC_EBADTYPE Type not found.
 * @return ::NC_ETYPDEFINED Type already defined.
 * @author Ed Hartnett
*/
int
NC4_insert_enum(int ncid, nc_type typeid1, const char *identifier, 
	       const void *value)
{
   NC_GRP_INFO_T *grp;
   NC_TYPE_INFO_T *type;
   char norm_name[NC_MAX_NAME + 1];
   int retval;

   LOG((2, "nc_insert_enum: ncid 0x%x, typeid %d identifier %s value %d", ncid, 
	typeid1, identifier, value));

   /* Check and normalize the name. */
   if ((retval = nc4_check_name(identifier, norm_name)))
      return retval;

   /* Find file metadata. */
   if ((retval = nc4_find_nc4_grp(ncid, &grp)))
      return retval;

   /* Find type metadata. */
   if ((retval = nc4_find_type(grp->nc4_info, typeid1, &type)))
      return retval;

   /* Did the user give us a good enum typeid? */
   if (!type || type->nc_type_class != NC_ENUM)
      return NC_EBADTYPE;

   /* If this type has already been written to the file, you can't
    * change it. */
   if (type->committed)
      return NC_ETYPDEFINED;

   /* Insert new field into this type's list of fields. */
   if ((retval = nc4_enum_member_add(&type->u.e.enum_member, type->size, 
				     norm_name, value)))
      return retval;
   type->u.e.num_members++;
   
   return NC_NOERR;
}

/**
 * @internal Insert one element into an already allocated vlen array
 * element.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param vlen_element The VLEN element to insert.
 * @param len Length of element in bytes.
 * @param data Element data.
 *
 * @return ::NC_NOERR No error.
 * @author Ed Hartnett
*/
int
NC4_put_vlen_element(int ncid, int typeid1, void *vlen_element, 
		    size_t len, const void *data)
{
   nc_vlen_t *tmp = (nc_vlen_t*)vlen_element;
   tmp->len = len;
   tmp->p = (void *)data;
   return NC_NOERR;
}

/**
 * @internal Insert one element into an already allocated vlen array
 * element.
 *
 * @param ncid File and group ID.
 * @param typeid1 Type ID.
 * @param vlen_element The VLEN element to insert.
 * @param len Length of element in bytes.
 * @param data Element data.
 *
 * @return ::NC_NOERR No error.
 * @author Ed Hartnett
*/
int
NC4_get_vlen_element(int ncid, int typeid1, const void *vlen_element, 
		    size_t *len, void *data)
{
   const nc_vlen_t *tmp = (nc_vlen_t*)vlen_element;
   int type_size = 4;

   *len = tmp->len;
   memcpy(data, tmp->p, tmp->len * type_size);
   return NC_NOERR;
}

