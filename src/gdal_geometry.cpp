#include "gdal_common.hpp"

#include "gdal_spatial_reference.hpp"
#include "gdal_coordinate_transformation.hpp"
#include "gdal_geometry.hpp"
#include "gdal_geometrycollection.hpp"
#include "gdal_point.hpp"
#include "gdal_linestring.hpp"
#include "gdal_linearring.hpp"
#include "gdal_polygon.hpp"
#include "gdal_multipoint.hpp"
#include "gdal_multilinestring.hpp"
#include "gdal_multipolygon.hpp"

#include "utils/fast_buffer.hpp"

#include <node_buffer.h>
#include <sstream>
#include <stdlib.h>
#include <ogr_core.h>

namespace node_gdal {

Persistent<FunctionTemplate> Geometry::constructor;

void Geometry::Initialize(Handle<Object> target)
{
	NanScope();

	Local<FunctionTemplate> lcons = NanNew<FunctionTemplate>(Geometry::New);
	lcons->InstanceTemplate()->SetInternalFieldCount(1);
	lcons->SetClassName(NanNew("Geometry"));

	//NODE_SET_METHOD(constructor, "fromWKBType", Geometry::create);
	NODE_SET_METHOD(lcons, "fromWKT", Geometry::createFromWkt);
	NODE_SET_METHOD(lcons, "fromWKB", Geometry::createFromWkb);
	NODE_SET_METHOD(lcons, "getName", Geometry::getName);
	NODE_SET_METHOD(lcons, "getConstructor", Geometry::getConstructor);

	NODE_SET_PROTOTYPE_METHOD(lcons, "toString", toString);
	NODE_SET_PROTOTYPE_METHOD(lcons, "toKML", exportToKML);
	NODE_SET_PROTOTYPE_METHOD(lcons, "toGML", exportToGML);
	NODE_SET_PROTOTYPE_METHOD(lcons, "toJSON", exportToJSON);
	NODE_SET_PROTOTYPE_METHOD(lcons, "toWKT", exportToWKT);
	NODE_SET_PROTOTYPE_METHOD(lcons, "toWKB", exportToWKB);
	NODE_SET_PROTOTYPE_METHOD(lcons, "isEmpty", isEmpty);
	NODE_SET_PROTOTYPE_METHOD(lcons, "isValid", isValid);
	NODE_SET_PROTOTYPE_METHOD(lcons, "isSimple", isSimple);
	NODE_SET_PROTOTYPE_METHOD(lcons, "isRing", isRing);
	NODE_SET_PROTOTYPE_METHOD(lcons, "clone", clone);
	NODE_SET_PROTOTYPE_METHOD(lcons, "empty", empty);
	NODE_SET_PROTOTYPE_METHOD(lcons, "closeRings", closeRings);
	NODE_SET_PROTOTYPE_METHOD(lcons, "intersects", intersects);
	NODE_SET_PROTOTYPE_METHOD(lcons, "equals", equals);
	NODE_SET_PROTOTYPE_METHOD(lcons, "disjoint", disjoint);
	NODE_SET_PROTOTYPE_METHOD(lcons, "touches", touches);
	NODE_SET_PROTOTYPE_METHOD(lcons, "crosses", crosses);
	NODE_SET_PROTOTYPE_METHOD(lcons, "within", within);
	NODE_SET_PROTOTYPE_METHOD(lcons, "contains", contains);
	NODE_SET_PROTOTYPE_METHOD(lcons, "overlaps", overlaps);
	NODE_SET_PROTOTYPE_METHOD(lcons, "boundary", boundary);
	NODE_SET_PROTOTYPE_METHOD(lcons, "distance", distance);
	NODE_SET_PROTOTYPE_METHOD(lcons, "convexHull", convexHull);
	NODE_SET_PROTOTYPE_METHOD(lcons, "buffer", buffer);
	NODE_SET_PROTOTYPE_METHOD(lcons, "intersection", intersection);
	NODE_SET_PROTOTYPE_METHOD(lcons, "union", unionGeometry);
	NODE_SET_PROTOTYPE_METHOD(lcons, "difference", difference);
	NODE_SET_PROTOTYPE_METHOD(lcons, "symDifference", symDifference);
	NODE_SET_PROTOTYPE_METHOD(lcons, "centroid", centroid);
	NODE_SET_PROTOTYPE_METHOD(lcons, "simplify", simplify);
	NODE_SET_PROTOTYPE_METHOD(lcons, "simplifyPreserveTopology", simplifyPreserveTopology);
	NODE_SET_PROTOTYPE_METHOD(lcons, "segmentize", segmentize);
	NODE_SET_PROTOTYPE_METHOD(lcons, "swapXY", swapXY);
	NODE_SET_PROTOTYPE_METHOD(lcons, "getEnvelope", getEnvelope);
	NODE_SET_PROTOTYPE_METHOD(lcons, "getEnvelope3D", getEnvelope3D);
	NODE_SET_PROTOTYPE_METHOD(lcons, "transform", transform);
	NODE_SET_PROTOTYPE_METHOD(lcons, "transformTo", transformTo);

	ATTR(lcons, "srs", srsGetter, srsSetter);
	ATTR(lcons, "wkbSize", wkbSizeGetter, READ_ONLY_SETTER);
	ATTR(lcons, "dimension", dimensionGetter, READ_ONLY_SETTER);
	ATTR(lcons, "coordinateDimension", coordinateDimensionGetter, READ_ONLY_SETTER);
	ATTR(lcons, "wkbType", typeGetter, READ_ONLY_SETTER);
	ATTR(lcons, "name", nameGetter, READ_ONLY_SETTER);

	target->Set(NanNew("Geometry"), lcons->GetFunction());

	NanAssignPersistent(constructor, lcons);
}

Geometry::Geometry(OGRGeometry *geom)
	: ObjectWrap(),
	  this_(geom),
	  owned_(true),
	  size_(0)
{
	LOG("Created Geometry [%p]", geom);
}

Geometry::Geometry()
	: ObjectWrap(),
	  this_(NULL),
	  owned_(true),
	  size_(0)
{
}

Geometry::~Geometry()
{
	if(this_) {
		LOG("Disposing Geometry [%p] (%s)", this_, owned_ ? "owned" : "unowned");
		if (owned_) {
			OGRGeometryFactory::destroyGeometry(this_);
			NanAdjustExternalMemory(-size_);
		}
		LOG("Disposed Geometry [%p]", this_)
		this_ = NULL;
	}
}

/**
 * @constructor
 * @class gdal.Geometry
 */
NAN_METHOD(Geometry::New)
{
	NanScope();
	Geometry *f;

	if (!args.IsConstructCall()) {
		NanThrowError("Cannot call constructor as function, you need to use 'new' keyword");
		NanReturnUndefined();
	}

	if (args[0]->IsExternal()) {
		Local<External> ext = args[0].As<External>();
		void* ptr = ext->Value();
		f = static_cast<Geometry *>(ptr);

	} else {
		NanThrowError("Geometry doesnt have a constructor, use Geometry.fromWKT(), Geometry.fromWKB() or type-specific constructor. ie. new ogr.Point()");
		NanReturnUndefined();
		//OGRwkbGeometryType geometry_type;
		//NODE_ARG_ENUM(0, "geometry type", OGRwkbGeometryType, geometry_type);
		//OGRGeometry *geom = OGRGeometryFactory::createGeometry(geometry_type);
		//f = new Geometry(geom);
	}

	f->Wrap(args.This());
	NanReturnValue(args.This());
}

Handle<Value> Geometry::New(OGRGeometry *geom)
{
	NanEscapableScope();
	return NanEscapeScope(Geometry::New(geom, true));
}

Handle<Value> Geometry::New(OGRGeometry *geom, bool owned)
{
	NanEscapableScope();

	if (!geom) {
		return NanEscapeScope(NanNull());
	}

	OGRwkbGeometryType type = getGeometryType_fixed(geom);
	type = wkbFlatten(type);

	switch (type) {
		case wkbPoint:
			return NanEscapeScope(Point::New(static_cast<OGRPoint*>(geom), owned));
		case wkbLineString:
			return NanEscapeScope(LineString::New(static_cast<OGRLineString*>(geom), owned));
		case wkbLinearRing:
			return NanEscapeScope(LinearRing::New(static_cast<OGRLinearRing*>(geom), owned));
		case wkbPolygon:
			return NanEscapeScope(Polygon::New(static_cast<OGRPolygon*>(geom), owned));
		case wkbGeometryCollection:
			return NanEscapeScope(GeometryCollection::New(static_cast<OGRGeometryCollection*>(geom), owned));
		case wkbMultiPoint:
			return NanEscapeScope(MultiPoint::New(static_cast<OGRMultiPoint*>(geom), owned));
		case wkbMultiLineString:
			return NanEscapeScope(MultiLineString::New(static_cast<OGRMultiLineString*>(geom), owned));
		case wkbMultiPolygon:
			return NanEscapeScope(MultiPolygon::New(static_cast<OGRMultiPolygon*>(geom), owned));
		default:
			NanThrowError("Tried to create unsupported geometry type");
			return NanEscapeScope(NanUndefined());
	}
}

OGRwkbGeometryType Geometry::getGeometryType_fixed(OGRGeometry* geom)
{
	//For some reason OGRLinearRing::getGeometryType uses OGRLineString's method...
	//meaning OGRLinearRing::getGeometryType returns wkbLineString

	//http://trac.osgeo.org/gdal/ticket/1755

	OGRwkbGeometryType type = geom->getGeometryType();

	if (std::string(geom->getGeometryName()) == "LINEARRING") {
		type = (OGRwkbGeometryType) (wkbLinearRing | (type & wkb25DBit));
	}

	return type;
}

NAN_METHOD(Geometry::toString)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	std::ostringstream ss;
	ss << "Geometry (" << geom->this_->getGeometryName() << ")";
	NanReturnValue(NanNew(ss.str().c_str()));
}


NODE_WRAPPED_METHOD(Geometry, closeRings, closeRings);
NODE_WRAPPED_METHOD(Geometry, empty, empty);
NODE_WRAPPED_METHOD(Geometry, swapXY, swapXY);
NODE_WRAPPED_METHOD_WITH_RESULT(Geometry, isEmpty, Boolean, IsEmpty);
NODE_WRAPPED_METHOD_WITH_RESULT(Geometry, isValid, Boolean, IsValid);
NODE_WRAPPED_METHOD_WITH_RESULT(Geometry, isSimple, Boolean, IsSimple);
NODE_WRAPPED_METHOD_WITH_RESULT(Geometry, isRing, Boolean, IsRing);
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, intersects, Boolean, Intersects, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, equals, Boolean, Equals, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, disjoint, Boolean, Disjoint, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, touches, Boolean, Touches, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, crosses, Boolean, Crosses, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, within, Boolean, Within, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, contains, Boolean, Contains, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, overlaps, Boolean, Overlaps, Geometry, "geometry to compare");
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(Geometry, distance, Number, Distance, Geometry, "geometry to use for distance calculation");
NODE_WRAPPED_METHOD_WITH_1_DOUBLE_PARAM(Geometry, segmentize, segmentize, "segment length");
NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(Geometry, transform, transform, CoordinateTransformation, "transform");
NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_WRAPPED_PARAM(Geometry, transformTo, transformTo, SpatialReference, "spatial reference");

/**
 * Clones the instance.
 *
 * @method clone
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::clone)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(Geometry::New(geom->this_->clone()));
}

/**
 * Compute convex hull.
 *
 * @method convexHull
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::convexHull)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(Geometry::New(geom->this_->ConvexHull()));
}

/**
 * Compute boundary.
 *
 * @method boundary
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::boundary)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(Geometry::New(geom->this_->Boundary()));
}

/**
 * Compute intersection with another geometry.
 *
 * @method intersection
 * @param {gdal.Geometry} geometry
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::intersection)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	Geometry *x = NULL;

	NODE_ARG_WRAPPED(0, "geometry to use for intersection", Geometry, x);

	NanReturnValue(Geometry::New(geom->this_->Intersection(x->this_)));
}

/**
 * Compute the union of this geometry with another.
 *
 * @method unionGeometry
 * @param {gdal.Geometry} geometry
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::unionGeometry)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	Geometry *x = NULL;

	NODE_ARG_WRAPPED(0, "geometry to use for union", Geometry, x);

	NanReturnValue(Geometry::New(geom->this_->Union(x->this_)));
}

/**
 * Compute the difference of this geometry with another.
 *
 * @method difference
 * @param {gdal.Geometry} geometry
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::difference)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	Geometry *x = NULL;

	NODE_ARG_WRAPPED(0, "geometry to use for difference", Geometry, x);

	NanReturnValue(Geometry::New(geom->this_->Difference(x->this_)));
}

/**
 * Computes the symmetric difference of this geometry and the second geometry.
 *
 * @method symDifference
 * @param {gdal.Geometry} geometry
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::symDifference)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	Geometry *x = NULL;

	NODE_ARG_WRAPPED(0, "geometry to use for symDifference", Geometry, x);

	NanReturnValue(Geometry::New(geom->this_->SymDifference(x->this_)));
}

/**
 * Reduces the geometry complexity.
 *
 * @method simplify
 * @param {Number} tolerance
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::simplify)
{
	NanScope();

	double tolerance;

	NODE_ARG_DOUBLE(0, "tolerance", tolerance);

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	NanReturnValue(Geometry::New(geom->this_->Simplify(tolerance)));
}

/**
 * Reduces the geometry complexity while preserving the topology.
 *
 * @method simplifyPreserveTopology
 * @param {Number} tolerance
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::simplifyPreserveTopology)
{
	NanScope();

	double tolerance;

	NODE_ARG_DOUBLE(0, "tolerance", tolerance);

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	NanReturnValue(Geometry::New(geom->this_->SimplifyPreserveTopology(tolerance)));
}

/**
 * Buffers the geometry by the given distance.
 *
 * @method buffer
 * @param {Number} distance
 * @param {integer} segments
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::buffer)
{
	NanScope();

	double distance;
	int number_of_segments = 30;

	NODE_ARG_DOUBLE(0, "distance", distance);
	NODE_ARG_INT_OPT(1, "number of segments", number_of_segments);

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	NanReturnValue(Geometry::New(geom->this_->Buffer(distance, number_of_segments)));
}

/**
 * Convert a geometry into well known text format.
 *
 * @method exportToWKT
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::exportToWKT)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	char *text = NULL;
	OGRErr err = geom->this_->exportToWkt(&text);

	if(err) {
		NODE_THROW_OGRERR(err);
		NanReturnUndefined();
	}
	if (text) {
		NanReturnValue(SafeString::New(text));
	}

	NanReturnUndefined();
}

/**
 * Convert a geometry into well known binary format.
 *
 * @method exportToWKT
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::exportToWKB)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	int size = geom->this_->WkbSize();
	unsigned char *data = (unsigned char*) malloc(size);

	//byte order
	OGRwkbByteOrder byte_order;
	std::string order = "MSB";
	NODE_ARG_OPT_STR(0, "byte order", order);
	if (order == "MSB") {
		byte_order = wkbXDR;
	} else if (order == "LSB") {
		byte_order = wkbNDR;
	} else {
		NanThrowError("byte order must be 'MSB' or 'LSB'");
		NanReturnUndefined();
	}

	#if GDAL_VERSION_MAJOR > 1 || (GDAL_VERSION_MINOR > 10)
	//wkb variant
	OGRwkbVariant wkb_variant;
	std::string variant = "OGC";
	NODE_ARG_OPT_STR(1, "wkb variant", variant);
	if (variant == "OGC") {
		wkb_variant = wkbVariantOgc;
	} else if (order == "ISO") {
		wkb_variant = wkbVariantIso;
	} else {
		NanThrowError("byte order must be 'OGC' or 'ISO'");
		NanReturnUndefined();
	}
	OGRErr err = geom->this_->exportToWkb(byte_order, data, wkb_variant);
	#else
	OGRErr err = geom->this_->exportToWkb(byte_order, data);
	#endif

	//^^ export to wkb and fill buffer ^^
	//TODO: avoid extra memcpy in FastBuffer::New and have exportToWkb write directly into buffer

	if(err) {
		free(data);
		NODE_THROW_OGRERR(err);
		NanReturnUndefined();
	}

	Handle<Value> result = FastBuffer::New(data, size);
	free(data);

	NanReturnValue(result);

}

/**
 * Convert a geometry into KML format.
 *
 * @method exportToKML
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::exportToKML)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	char *text = geom->this_->exportToKML();
	if (text) {
		Handle<Value> result = NanNew(text);
		CPLFree(text);
		NanReturnValue(result);
	}

	NanReturnUndefined();
}

/**
 * Convert a geometry into GML format.
 *
 * @method exportToGML
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::exportToGML)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	char *text = geom->this_->exportToGML();
	if (text) {
		Handle<Value> result = NanNew(text);
		CPLFree(text);
		NanReturnValue(result);
	}

	NanReturnUndefined();
}

/**
 * Convert a geometry into JSON format.
 *
 * @method exportToJSON
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::exportToJSON)
{
	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	char *text = geom->this_->exportToJson();
	if (text) {
		Handle<Value> result = NanNew(text);
		CPLFree(text);
		NanReturnValue(result);
	}

	NanReturnUndefined();
}

/**
 * Compute the centroid of the geometry.
 *
 * @method centroid
 * @returns gdal.Point
 */
NAN_METHOD(Geometry::centroid)
{
	// The Centroid method wants the caller to create the point to fill in. Instead
	// of requiring the caller to create the point geometry to fill in, we new up an
	// OGRPoint and put the result into it and return that.
	NanScope();
	OGRPoint *point = new OGRPoint();

	ObjectWrap::Unwrap<Geometry>(args.This())->this_->Centroid(point);

	NanReturnValue(Point::New(point));
}

/**
 * Computes the bounding box (envelope).
 *
 * @method getEnvelope
 * @returns Object
 */
NAN_METHOD(Geometry::getEnvelope)
{
	//returns object containing boundaries until complete OGREnvelope binding is built

	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	OGREnvelope *envelope = new OGREnvelope();
	geom->this_->getEnvelope(envelope);

	Local<Object> obj = NanNew<Object>();
	obj->Set(NanNew("minX"), NanNew<Number>(envelope->MinX));
	obj->Set(NanNew("maxX"), NanNew<Number>(envelope->MaxX));
	obj->Set(NanNew("minY"), NanNew<Number>(envelope->MinY));
	obj->Set(NanNew("maxY"), NanNew<Number>(envelope->MaxY));

	delete envelope;

	NanReturnValue(obj);
}

/**
 * Computes the 3D bounding box (envelope).
 *
 * @method getEnvelope
 * @returns Object
 */
NAN_METHOD(Geometry::getEnvelope3D)
{
	//returns object containing boundaries until complete OGREnvelope binding is built

	NanScope();

	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	OGREnvelope3D *envelope = new OGREnvelope3D();
	geom->this_->getEnvelope(envelope);

	Local<Object> obj = NanNew<Object>();
	obj->Set(NanNew("minX"), NanNew<Number>(envelope->MinX));
	obj->Set(NanNew("maxX"), NanNew<Number>(envelope->MaxX));
	obj->Set(NanNew("minY"), NanNew<Number>(envelope->MinY));
	obj->Set(NanNew("maxY"), NanNew<Number>(envelope->MaxY));
	obj->Set(NanNew("minZ"), NanNew<Number>(envelope->MinZ));
	obj->Set(NanNew("maxZ"), NanNew<Number>(envelope->MaxZ));

	delete envelope;

	NanReturnValue(obj);
}

// --- JS static methods (OGRGeometryFactory) ---

/**
 * Creates a Geometry from a WKT string.
 *
 * @static
 * @method createFromWkt
 * @param {String} wkt
 * @param {gdal.SpatialReference} [src]
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::createFromWkt)
{
	NanScope();

	std::string wkt_string;
	SpatialReference *srs = NULL;

	NODE_ARG_STR(0, "wkt", wkt_string);
	NODE_ARG_WRAPPED_OPT(1, "srs", SpatialReference, srs);

	char *wkt = (char*) wkt_string.c_str();
	OGRGeometry *geom = NULL;
	OGRSpatialReference *ogr_srs = NULL;
	if (srs) {
		ogr_srs = srs->get();
	}

	OGRErr err = OGRGeometryFactory::createFromWkt(&wkt, ogr_srs, &geom);
	if (err) {
		NODE_THROW_OGRERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(Geometry::New(geom, true));
}

/**
 * Creates a Geometry from a WKB string.
 *
 * @static
 * @method createFromWkb
 * @param {Object} wkb
 * @param {gdal.SpatialReference} [src]
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::createFromWkb)
{
	NanScope();

	std::string wkb_string;
	SpatialReference *srs = NULL;

	Handle<Object> wkb_obj;
	NODE_ARG_OBJECT(0, "wkb", wkb_obj);
	NODE_ARG_WRAPPED_OPT(1, "srs", SpatialReference, srs);

	std::string obj_type = *NanUtf8String(wkb_obj->GetConstructorName());

	if(obj_type != "Buffer"){
		NanThrowError("Argument must be a buffer object");
		NanReturnUndefined();
	}

	unsigned char* data = (unsigned char *) Buffer::Data(wkb_obj);
	size_t length = Buffer::Length(wkb_obj);

	OGRGeometry *geom = NULL;
	OGRSpatialReference *ogr_srs = NULL;
	if (srs) {
		ogr_srs = srs->get();
	}

	OGRErr err = OGRGeometryFactory::createFromWkb(data, ogr_srs, &geom, length);
	if (err) {
		NODE_THROW_OGRERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(Geometry::New(geom, true));
}

/**
 * Creates an empty Geometry from a WKB type.
 *
 * @static
 * @method create
 * @param {Object} type WKB geometry type ({{#crossLink "Constants (WKB)"}}available options{{/crossLink}})
 * @returns gdal.Geometry
 */
NAN_METHOD(Geometry::create)
{
	NanScope();

	OGRwkbGeometryType type = wkbUnknown;
	NODE_ARG_ENUM(0, "type", OGRwkbGeometryType, type);

	NanReturnValue(Geometry::New(OGRGeometryFactory::createGeometry(type), true));
}

/**
 * @attribute srs
 * @type gdal.SpatialReference
 */
NAN_GETTER(Geometry::srsGetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(SpatialReference::New(geom->this_->getSpatialReference(), false));
}

NAN_SETTER(Geometry::srsSetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());

	OGRSpatialReference *srs = NULL;
	if (IS_WRAPPED(value, SpatialReference)) {
		SpatialReference *srs_obj = ObjectWrap::Unwrap<SpatialReference>(value.As<Object>());
		srs = srs_obj->get();
	} else if (!value->IsNull() && !value->IsUndefined()) {
		NanThrowError("srs must be SpatialReference object");
		return;
	}

	geom->this_->assignSpatialReference(srs);
}

/**
 * @attribute name
 * @type String
 */
NAN_GETTER(Geometry::nameGetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(SafeString::New(geom->this_->getGeometryName()));
}

/**
 * @attribute type
 * @type Function
 */
NAN_GETTER(Geometry::typeGetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(NanNew<Integer>(getGeometryType_fixed(geom->this_)));
}

/**
 * @attribute wkbSize
 * @type Integer
 */
NAN_GETTER(Geometry::wkbSizeGetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(NanNew<Integer>(geom->this_->WkbSize()));
}

NAN_GETTER(Geometry::dimensionGetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(NanNew<Integer>(geom->this_->getDimension()));
}

NAN_GETTER(Geometry::coordinateDimensionGetter)
{
	NanScope();
	Geometry *geom = ObjectWrap::Unwrap<Geometry>(args.This());
	NanReturnValue(NanNew<Integer>(geom->this_->getCoordinateDimension()));
}

Handle<Value> Geometry::getConstructor(OGRwkbGeometryType type){
	NanEscapableScope();

	type = wkbFlatten(type);
	switch (type) {
		case wkbPoint:              return NanEscapeScope(NanNew(Point::constructor)->GetFunction());
		case wkbLineString:         return NanEscapeScope(NanNew(LineString::constructor)->GetFunction());
		case wkbLinearRing:         return NanEscapeScope(NanNew(LinearRing::constructor)->GetFunction());
		case wkbPolygon:            return NanEscapeScope(NanNew(Polygon::constructor)->GetFunction());
		case wkbGeometryCollection: return NanEscapeScope(NanNew(GeometryCollection::constructor)->GetFunction());
		case wkbMultiPoint:         return NanEscapeScope(NanNew(MultiPoint::constructor)->GetFunction());
		case wkbMultiLineString:    return NanEscapeScope(NanNew(MultiLineString::constructor)->GetFunction());
		case wkbMultiPolygon:       return NanEscapeScope(NanNew(MultiPolygon::constructor)->GetFunction());
		default:                    return NanEscapeScope(NanNull());
	}
}

/**
 * Returns the Geometry subclass that matches the
 * given WKB geometry type.
 *
 * @static
 * @method getConstructor
 * @param {Integer} type WKB geometry type ({{#crossLink "Constants (WKB)"}}available options{{/crossLink}})
 * @returns Function
 */
NAN_METHOD(Geometry::getConstructor){
	NanScope();
	OGRwkbGeometryType type;
	NODE_ARG_ENUM(0, "wkbType", OGRwkbGeometryType, type);
	NanReturnValue(getConstructor(type));
}

/**
 * Returns the Geometry subclass name that matches the
 * given WKB geometry type.
 *
 * @static
 * @method getName
 * @param {Integer} type WKB geometry type ({{#crossLink "Constants (WKB)"}}available options{{/crossLink}})
 * @returns String
 */
NAN_METHOD(Geometry::getName){
	NanScope();
	OGRwkbGeometryType type;
	NODE_ARG_ENUM(0, "wkbType", OGRwkbGeometryType, type);
	NanReturnValue(SafeString::New(OGRGeometryTypeToName(type)));
}

} // namespace node_gdal