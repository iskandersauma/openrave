// -*- coding: utf-8 -*-
// Copyright (C) 2012-2016 Rosen Diankov, Gustavo Puche, OpenGrasp Team
//
// OpenRAVE Qt/OpenSceneGraph Viewer is licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/*! --------------------------------------------------------------------
   \file   Item.cpp
   \brief  Abstract base class for an Item
   -------------------------------------------------------------------- */
#include "qtosg.h"
#include "osgrenderitem.h"

#include <osgUtil/SmoothingVisitor>
#include <osg/BlendFunc>
#include <osg/PolygonOffset>
#include <osg/LineStipple>

namespace qtosgrave {



// Visitor to return the coordinates of a node with respect to another node
class WorldCoordOfNodeVisitor : public osg::NodeVisitor
{
public:
    WorldCoordOfNodeVisitor(OSGNodePtr osgSceneRoot) : osg::NodeVisitor(NodeVisitor::TRAVERSE_PARENTS), _osgSceneRoot(osgSceneRoot), done(false) {
    }
    virtual void apply(osg::Node &node) {
        if( !done ) {
            if( &node == _osgSceneRoot.get() ) {
                // found the path, so get the world coordinate system
                wcMatrix.set( osg::computeLocalToWorld(getNodePath()));
                done = true;
            }
            else {
                traverse(node);
            }
        }
    }

    void Reset() {
        done = false;
        wcMatrix.makeIdentity();
    }

    bool IsDone() {
        return done;
    }

    osg::Matrix wcMatrix;
private:
    OSGNodePtr _osgSceneRoot;
    bool done;
};

Item::Item(OSGGroupPtr osgSceneRoot) : _osgSceneRoot(osgSceneRoot)
{
    // set up the Inventor nodes
    _osgWorldTransform = new osg::MatrixTransform;
    _osgdata = new osg::Group();
    _osgWorldTransform->addChild(_osgdata);

    _osgSceneRoot->addChild(_osgWorldTransform);
}

Item::~Item()
{
    _osgSceneRoot->removeChild(_osgWorldTransform); // should remove all references
}

bool Item::ContainsOSGNode(OSGNodePtr pNode)
{
    FindNode search(pNode);
    search.apply(*_osgWorldTransform);
    return search.IsFound();
}

void Item::SetVisualizationMode(const std::string& visualizationmode)
{
    if( _visualizationmode != visualizationmode ) {

        // have to undo the previous mode
        if( !!_osgwireframe ) {
            _osgWorldTransform->removeChild(_osgwireframe);
            _osgwireframe.release();
        }

        // start the new node
        _visualizationmode = visualizationmode;

        if( _visualizationmode == "selected" ) {
            _osgwireframe = new osg::Group;
            _osgWorldTransform->addChild(_osgwireframe);
            _osgwireframe->addChild(_osgdata);

            // set up the state so that the underlying color is not seen through
            // and that the drawing mode is changed to wireframe, and a polygon offset
            // is added to ensure that we see the wireframe itself, and turn off
            // so texturing too.
            osg::ref_ptr<osg::StateSet> stateset = new osg::StateSet;
            osg::ref_ptr<osg::PolygonOffset> polyoffset = new osg::PolygonOffset;
            polyoffset->setFactor(-1.0f);
            polyoffset->setUnits(-1.0f);
            osg::ref_ptr<osg::PolygonMode> polymode = new osg::PolygonMode;
            polymode->setMode(osg::PolygonMode::FRONT_AND_BACK,osg::PolygonMode::LINE);
            stateset->setAttributeAndModes(polyoffset,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
            stateset->setAttributeAndModes(polymode,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);

#if 1
            osg::ref_ptr<osg::Material> material = new osg::Material;
            material->setDiffuse(osg::Material::FRONT_AND_BACK,osg::Vec4f(0,1,0,1));
            material->setAmbient(osg::Material::FRONT_AND_BACK,osg::Vec4f(0,1,0,1));

            stateset->setAttributeAndModes(material,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
            stateset->setMode(GL_LIGHTING,osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF);
#else
            // version which sets the color of the wireframe.
            osg::ref_ptr<osg::Material> material = new osg::Material;
            material->setColorMode(osg::Material::OFF); // switch glColor usage off
            // turn all lighting off
            material->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0,0.0f,0.0f,1.0f));
            material->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0,0.0f,0.0f,1.0f));
            material->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0,0.0f,0.0f,1.0f));
            // except emission... in which we set the color we desire
            material->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.0,1.0f,0.0f,1.0f));
            stateset->setAttributeAndModes(material,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
            stateset->setMode(GL_LIGHTING,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
#endif

            stateset->setTextureMode(0,GL_TEXTURE_2D,osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF);

            osg::ref_ptr<osg::LineStipple> linestipple = new osg::LineStipple;
            linestipple->setFactor(1);
            linestipple->setPattern(0xf0f0);
            stateset->setAttributeAndModes(linestipple,osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
            
            _osgwireframe->setStateSet(stateset);
        }
        else if( _visualizationmode.size() > 0 ) {
            RAVELOG_INFO_FORMAT("unknown visualization type %s", visualizationmode);
        }
    }
}

//void Item::SetGeomVisibility(bool bFlag)
//{
//    if (bFlag) {
//        _osgdata->setAllChildrenOn();
//    }
//    else {
//        _osgdata->setAllChildrenOff();
//    }
//}

/// KinBodyItem class
KinBodyItem::KinBodyItem(OSGGroupPtr osgSceneRoot, KinBodyPtr pbody, ViewGeometry viewmode) : Item(osgSceneRoot), _viewmode(viewmode)
{
    BOOST_ASSERT( !!pbody );
    _pbody = pbody;
    bGrabbed = false;
    _userdata = 0;
    _bReload = false;
    _bDrawStateChanged = false;
    _environmentid = pbody->GetEnvironmentId();
    _geometrycallback = pbody->RegisterChangeCallback(KinBody::Prop_LinkGeometry, boost::bind(&KinBodyItem::_HandleGeometryChangedCallback,this));
    _drawcallback = pbody->RegisterChangeCallback(KinBody::Prop_LinkDraw, boost::bind(&KinBodyItem::_HandleDrawChangedCallback,this));
}

KinBodyItem::~KinBodyItem()
{
    _veclinks.clear();
}

void KinBodyItem::Load()
{
    _osgWorldTransform->setUserData(new OSGItemUserData(shared_from_this()));

    // Sets name of Robot or Kinbody
    _osgdata->setName(_pbody->GetName());
    _osgdata->removeChildren(0, _osgdata->getNumChildren()); // have to remove all the children before creating a new mesh

    _veclinks.resize(0);

    Transform tbody = _pbody->GetTransform();
    Transform tbodyinv = tbody.inverse();
    SetMatrixTransform(*_osgWorldTransform, tbody);

    //  Extract geometry
    FOREACHC(itlink, _pbody->GetLinks()) {
        KinBody::LinkPtr porlink = *itlink;
        OSGGroupPtr posglinkroot = new osg::Group();
        posglinkroot->setName(str(boost::format("link%d")%porlink->GetIndex()));

        OSGMatrixTransformPtr posglinktrans = new osg::MatrixTransform();
        SetMatrixTransform(*posglinktrans, tbodyinv * porlink->GetTransform());
        posglinktrans->setName(str(boost::format("link%dtrans")%porlink->GetIndex()));

        posglinkroot->addChild(posglinktrans);

//        std::vector< boost::shared_ptr<KinBody::Link> > vParentLinks;
//        porlink->GetParentLinks(vParentLinks);
//        if( vParentLinks.size() > 0 ) {
//            // need to set transform with respect to parent since osg transforms
//            SetMatrixTransform(*posglink, vParentLinks[0]->GetTransform().inverse() * porlink->GetTransform());
//        }
//        else {
//            // no parent, so use the root link's transform
//
//        }

        _veclinks.push_back(LinkNodes(posglinkroot, posglinktrans));

        for(size_t igeom = 0; igeom < porlink->GetGeometries().size(); ++igeom) {
            KinBody::Link::GeometryPtr orgeom = porlink->GetGeometries()[igeom];
            if( !orgeom->IsVisible() && _viewmode == VG_RenderOnly ) {
                continue;
            }

            OSGGroupPtr pgeometrydata;
            OSGMatrixTransformPtr pgeometryroot = new osg::MatrixTransform();
            SetMatrixTransform(*pgeometryroot, orgeom->GetTransform());

            // open
            bool bSucceeded = false;
            if( _viewmode == VG_RenderOnly || _viewmode == VG_RenderCollision ) {
                string extension;
                if( orgeom->GetRenderFilename().find("__norenderif__:") == 0 ) {
                    string ignoreextension = orgeom->GetRenderFilename().substr(15);
                    if( ignoreextension == "wrl" || extension == "iv" || extension == "vrml" ) {
                        continue;
                    }
                }
                if( orgeom->GetRenderFilename().find_last_of('.') != string::npos ) {
                    extension = orgeom->GetRenderFilename().substr(orgeom->GetRenderFilename().find_last_of('.')+1);
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                }
                if( extension == "wrl" || extension == "iv" || extension == "vrml" ) {
                    OSGNodePtr loadedModel;
                    osg::Matrix mRotate, mS;

                    mRotate.makeRotate(-osg::PI/2,osg::Vec3f(1.0f,0.0f,0.0f));

                    mS.makeScale(orgeom->GetRenderScale().x, orgeom->GetRenderScale().y, orgeom->GetRenderScale().z);

                    pgeometryroot->preMult(mS);
                    pgeometryroot->preMult(mRotate);

                    loadedModel = osgDB::readNodeFile(orgeom->GetRenderFilename());

                    pgeometrydata = loadedModel->asGroup();
                    osg::StateSet* state = pgeometrydata->getOrCreateStateSet();
                    state->setMode(GL_RESCALE_NORMAL,osg::StateAttribute::ON);

                    bSucceeded = true;
                }
            }

            if( !bSucceeded || _viewmode == VG_RenderCollision ) {
                float x,y,z,w;

                osg::ref_ptr<osg::Material> mat = new osg::Material;
                float transparency = orgeom->GetTransparency();
//                if( _viewmode == VG_RenderCollision && (bSucceeded || !orgeom->IsVisible()) ) {
//                    mat->setDiffuse(osg::Material::FRONT_AND_BACK,osg::Vec4f(0.6f,0.6f,1.0f,1.0f));
//                    mat->setAmbient(osg::Material::FRONT_AND_BACK,osg::Vec4f(0.4f,0.4f,1.0f,1.0f));
//                    transparency = 0.5f;
//                }

                // create custom
                if( !pgeometrydata ) {
                    pgeometrydata = new osg::Group();
                }

                // set a diffuse color
                osg::ref_ptr<osg::StateSet> state = pgeometrydata->getOrCreateStateSet();

                x = orgeom->GetDiffuseColor().x;
                y = orgeom->GetDiffuseColor().y;
                z = orgeom->GetDiffuseColor().z;
                w = 1;

                mat->setDiffuse( osg::Material::FRONT, osg::Vec4f(x,y,z,w) );

                RAVELOG_VERBOSE_FORMAT("Diffuse color= %f %f %f\n",x%y%z);

                x = orgeom->GetAmbientColor().x;
                y = orgeom->GetAmbientColor().y;
                z = orgeom->GetAmbientColor().z;
                w = 1.0f;

                mat->setAmbient( osg::Material::FRONT_AND_BACK, osg::Vec4f(x,y,z,w) );

                //mat->setShininess( osg::Material::FRONT, 25.0);
                mat->setEmission(osg::Material::FRONT, osg::Vec4(0.0, 0.0, 0.0, 1.0));

                // getting the object to be displayed with transparency
                if (transparency > 0) {
                    mat->setTransparency(osg::Material::FRONT_AND_BACK, transparency);

                    //state->setRenderBinDetails(0, "transparent");
                    state->setMode(GL_BLEND, osg::StateAttribute::ON);
                    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
                    state->setAttributeAndModes(new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA ));

                }
                state->setAttributeAndModes(mat, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
                //pgeometrydata->setStateSet(state);

                switch(orgeom->GetType()) {
                //  Geometry is defined like a Sphere
                case GT_Sphere: {
                    osg::Sphere* s = new osg::Sphere();
                    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
                    s->setRadius(orgeom->GetSphereRadius());
                    osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(s);
                    geode->addDrawable(sd.get());

                    pgeometrydata->addChild(geode.get());
                    break;
                }
                //  Geometry is defined like a Box
                case GT_Box: {
                    Vector v;
                    osg::ref_ptr<osg::Box> box = new osg::Box();
                    box->setHalfLengths(osg::Vec3f(orgeom->GetBoxExtents().x,orgeom->GetBoxExtents().y,orgeom->GetBoxExtents().z));

                    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
                    osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(box.get());
                    geode->addDrawable(sd.get());

                    pgeometrydata->addChild(geode.get());
                    break;
                }
                //  Geometry is defined like a Cylinder
                case GT_Cylinder: {
                    // make SoCylinder point towards z, not y
                    osg::Cylinder* cy = new osg::Cylinder();
                    cy->setRadius(orgeom->GetCylinderRadius());
                    cy->setHeight(orgeom->GetCylinderHeight());
                    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
                    osg::ref_ptr<osg::ShapeDrawable> sd = new osg::ShapeDrawable(cy);
                    geode->addDrawable(sd.get());
                    pgeometrydata->addChild(geode.get());
                    break;
                }
                //  Extract geometry from collision Mesh
                case GT_Container:
                case GT_TriMesh: {
                    // make triangleMesh
                    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

                    //geom->setColorBinding(osg::Geometry::BIND_OVERALL); // need to call geom->setColorArray first

                    const TriMesh& mesh = orgeom->GetCollisionMesh();
                    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
                    vertices->reserveArray(mesh.vertices.size());
                    for(size_t i = 0; i < mesh.vertices.size(); ++i) {
                        RaveVector<float> v = mesh.vertices[i];
                        vertices->push_back(osg::Vec3(v.x, v.y, v.z));
                    }
                    geom->setVertexArray(vertices.get());


                    osg::DrawElementsUInt* geom_prim = new osg::DrawElementsUInt(osg::PrimitiveSet::TRIANGLES, mesh.indices.size());
                    for(size_t i = 0; i < mesh.indices.size(); ++i) {
                        (*geom_prim)[i] = mesh.indices[i];
                    }
                    geom->addPrimitiveSet(geom_prim);

                    osgUtil::SmoothingVisitor::smooth(*geom); // compute vertex normals
                    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
                    geode->addDrawable(geom);
                    pgeometrydata->addChild(geode);
                    break;
                }
                default:
                    break;
                }
            }

            if( !!pgeometrydata ) {
                pgeometrydata->setName(str(boost::format("geomdata%d")%igeom));
                pgeometryroot->addChild(pgeometrydata);
                pgeometryroot->setName(str(boost::format("geom%d")%igeom));
                //  Apply external transform to local transform
                posglinktrans->addChild(pgeometryroot);
            }
        }
    }

    //RAVELOG_VERBOSE_FORMAT("Number of links added = %d", _veclinks.size());

    //  Is an object without joints
    if (_pbody->GetJoints().size() < 1) {
        _osgdata->addChild(_veclinks.at(0).first);
    }
    //  Object with joints
    else {
        std::vector<uint8_t> addedlinks(_pbody->GetLinks().size(), 0);

        //  Assemble link hierarchy
        FOREACH(itjoint, _pbody->GetDependencyOrderedJoints()) {
            if( addedlinks[(*itjoint)->GetHierarchyChildLink()->GetIndex()] == 0 ) {
                OSGGroupPtr parent = _veclinks.at((*itjoint)->GetHierarchyParentLink()->GetIndex()).first;
                OSGGroupPtr child = _veclinks.at(((*itjoint)->GetHierarchyChildLink()->GetIndex())).first;
                parent->addChild(child);
                addedlinks.at((*itjoint)->GetHierarchyChildLink()->GetIndex()) = 1;
            }
            else {
                // already set, cannot set twice...
            }
        }

        //  Assemble passive joints
        FOREACH(itjoint, _pbody->GetPassiveJoints()) {
            if( addedlinks[(*itjoint)->GetHierarchyChildLink()->GetIndex()] == 0 ) {
                OSGGroupPtr parent = _veclinks.at((*itjoint)->GetHierarchyParentLink()->GetIndex()).first;
                OSGGroupPtr child = _veclinks.at((*itjoint)->GetHierarchyChildLink()->GetIndex()).first;
                parent->addChild(child);
                addedlinks.at((*itjoint)->GetHierarchyChildLink()->GetIndex()) = 1;
            }
            else {
                // already set, cannot set twice...
            }
        }

        // have to add the left over links to the root group
        for(size_t ilink = 0; ilink < addedlinks.size(); ++ilink) {
            if( addedlinks[ilink] == 0 ) {
                _osgdata->addChild(_veclinks.at(ilink).first);
            }
        }
    }

    _bReload = false;
    _bDrawStateChanged = false;
}

void KinBodyItem::_PrintMatrix(osg::Matrix& m)
{
    for (size_t i = 0; i < 4; i++) {
        RAVELOG_WARN("Line '%d'= %f %f %f %f\n",i,m(i,0),m(i,1),m(i,2),m(i,3));
    }
}

void KinBodyItem::_PrintSceneGraph(const std::string& currLevel, OSGNodePtr currNode)
{
    std::string level;
    level = currLevel;

    // check to see if we have a valid (non-NULL) node.
    // if we do have a null node, return NULL.
    if ( !!currNode) {
        level = level + "-";
        RAVELOG_VERBOSE_FORMAT("|%sNode class:%s (%s)\n",currLevel%currNode->className()%currNode->getName());
        OSGGroupPtr currGroup = currNode->asGroup(); // returns NULL if not a group.
        if ( !!currGroup ) {
            for (unsigned int i = 0; i < currGroup->getNumChildren(); i++) {
                _PrintSceneGraph(level,currGroup->getChild(i));
            }
        }
    }
}

void KinBodyItem::_PrintNodeFeatures(OSGNodePtr node)
{
//  RAVELOG_VERBOSE("----->>>> printNodeFeatures(node)\n");
//  osg::StateSet* state;
//  osg::Material* mat;
//  osg::Light  *light;
//  osg::Geode  *geode;


//  for (size_t i = 0; i < node->asGroup()->getNumChildren(); i++)
//  {
//    geode = node->asGroup()->getChild(i)->asGroup()->getChild(0)->asGeode();
//    state = geode->getDrawable(0)->getOrCreateStateSet();
//    RAVELOG_VERBOSE("Number of Attributes = %d\n",state->getAttributeList().size());
//    RAVELOG_VERBOSE("Number of Modes = %d\n",state->getModeList().size());

//    mat = (osg::Material*)state->getAttribute(osg::StateAttribute::MATERIAL);
//    if (!!mat)
//    {
//      RAVELOG_INFO("SHININESS= %f\n",mat->getShininess(osg::Material::FRONT));
//      RAVELOG_INFO("Color Mode = %d\n",mat->getColorMode());
//    }
//    if (!!state)
//    {
//      RAVELOG_DEBUG("GEODE has StateSet\n");
//      for (osg::StateSet::AttributeList::iterator it = state->getAttributeList().begin();
//          it != state->getAttributeList().end();
//          it++)
//      {
//        RAVELOG_VERBOSE("Attribute Type: %d\n",(*it).first.first);
//      }
//      for (osg::StateSet::ModeList::iterator it = state->getModeList().begin();
//          it != state->getModeList().end();
//          it++)
//      {
//
//      }
//    }
//    else
//    {
//      RAVELOG_DEBUG("GEODE does NOT have StateSet\n");
//    }
//  }
}

/// Generate normals
//osg::ref_ptr<osg::Vec3Array> KinBodyItem::_GenerateNormals(const TriMesh& trimesh)
//{
//    osg::ref_ptr<osg::Vec3Array> normals(new osg::Vec3Array());
//    normals->reserveArray(trimesh.indices.size()/3);
//    unsigned int fi = 0;
//    while (fi+2 < trimesh.indices.size() ) {
//        // Edge vectors
//        Vector v0 = trimesh.vertices.at(trimesh.indices[fi]);
//        Vector v1 = trimesh.vertices.at(trimesh.indices[fi+1]);
//        Vector v2 = trimesh.vertices.at(trimesh.indices[fi+2]);
//
//        Vector e0 = v1 - v0;
//        Vector e1 = v2 - v0;
//
//        // Cross-product of e0,e1
//        Vector vn = e0.cross(e1);
//        vn.normalize3();
//        // Add to per-face normals
//        normals->push_back(osg::Vec3(vn.x,vn.y,vn.z));
//        fi+=3;
//    }
//
//    return normals;
//}

void KinBodyItem::_HandleGeometryChangedCallback()
{
    _bReload = true;
}

void KinBodyItem::_HandleDrawChangedCallback()
{
    _bDrawStateChanged = true;
}

bool KinBodyItem::UpdateFromOSG()
{
    if( !_pbody ) {
        return false;
    }
    vector<Transform> vtrans(_veclinks.size());

    WorldCoordOfNodeVisitor visitor(_osgSceneRoot);
    _osgWorldTransform->accept(visitor);
    if( !visitor.IsDone() ) {
        RAVELOG_WARN_FORMAT("node %s might not be attached to the viewer root", _name);
        return false;
    }

    Transform tglob = GetRaveTransformFromMatrix(visitor.wcMatrix);
    WorldCoordOfNodeVisitor linkvisitor(_osgdata);

    // need to use the WorldCoordOfNodeVisitor for getting the link transforms with respect to _osgWorldTransform since there could be draggers in between
    for(size_t ilink = 0; ilink < vtrans.size(); ++ilink) {
        linkvisitor.Reset();
        _veclinks.at(ilink).second->accept(linkvisitor);
        if( linkvisitor.IsDone() ) {
            vtrans[ilink] = tglob * GetRaveTransformFromMatrix(linkvisitor.wcMatrix);
        }
        else {
            Transform tlinkrel = GetRaveTransform(*_veclinks.at(ilink).second);
            vtrans[ilink] = tglob * tlinkrel;
        }
    }

    boost::shared_ptr<EnvironmentMutex::scoped_try_lock> lockenv = LockEnvironmentWithTimeout(_pbody->GetEnv(), 50000);
    if( !!lockenv ) {
        _pbody->SetLinkTransformations(vtrans,_vjointvalues);
        _pbody->GetLinkTransformations(_vtrans,_vjointvalues);
    }
    else {
        RAVELOG_WARN("failed to acquire environment lock for updating body (viewer updates might be choppy, otherwise this does not affect internal openrave state)\n");
    }
    return true;
}

void KinBodyItem::GetDOFValues(vector<dReal>& vjoints) const
{
    boost::mutex::scoped_lock lock(_mutexjoints);
    vjoints = _vjointvalues;
}

void KinBodyItem::GetLinkTransformations(vector<Transform>& vtrans, std::vector<dReal>& vdofvalues) const
{
    boost::mutex::scoped_lock lock(_mutexjoints);
    vtrans = _vtrans;
    vdofvalues = _vjointvalues;
}

bool KinBodyItem::UpdateFromModel()
{
    if( !_pbody ) {
        return false;
    }
    vector<Transform> vtrans;
    vector<dReal> vjointvalues;

    {
        boost::shared_ptr<EnvironmentMutex::scoped_try_lock> lockenv = LockEnvironmentWithTimeout(_pbody->GetEnv(), 50000);
        if( !lockenv ) {
            return false;
        }
        if( _bReload || _bDrawStateChanged ) {
            Load();
        }
        else {
            _osgdata->setName(_pbody->GetName());
        }

        // make sure the body is still present!
        if( _pbody->GetEnv()->GetBodyFromEnvironmentId(_environmentid) == _pbody ) {
            _pbody->GetLinkTransformations(_vtrans, _vjointvalues);
            _pbody->GetDOFValues(vjointvalues);
        }
        else {
            _pbody.reset();
        }
    }

    return UpdateFromModel(vjointvalues,vtrans);
}

bool KinBodyItem::UpdateFromModel(const vector<dReal>& vjointvalues, const vector<Transform>& vtrans)
{
    OSGMatrixTransformPtr mtransform;
    if(!_pbody ) {
        // don't update, physics is disabled anyway
        return false;
    }

    if( _bReload || _bDrawStateChanged ) {
        EnvironmentMutex::scoped_try_lock lockenv(_pbody->GetEnv()->GetMutex());
        if( !!lockenv ) {
            if( _bReload || _bDrawStateChanged ) {
                Load();
            }
        }
    }

    boost::mutex::scoped_lock lock(_mutexjoints);
    _vjointvalues = vjointvalues;
    _vtrans = vtrans;

    if( _vtrans.size() == 0 || _veclinks.size() != _vtrans.size() ) {
        // something's wrong, so just return
        return false;
    }

    // Global transform
    Transform tglob = _vtrans.at(0); //_pbody->GetCenterOfMass();

    // _osgWorldTransform might not be rooted at the world frame directly, so have to first set it to the identity and then take its world offset
    osg::Matrixd ident; ident.makeIdentity();
    _osgWorldTransform->setMatrix(ident);

    WorldCoordOfNodeVisitor visitor(_osgSceneRoot);
    _osgWorldTransform->accept(visitor);
    if( !visitor.IsDone() ) {
        visitor.wcMatrix.makeIdentity();
    }

    Transform tworldoffset = GetRaveTransformFromMatrix(visitor.wcMatrix);
    SetMatrixTransform(*_osgWorldTransform, tworldoffset.inverse() * tglob);

    //  Link iterator
    WorldCoordOfNodeVisitor linkvisitor(_osgdata);

    for(size_t ilink = 0; ilink < _veclinks.size(); ++ilink) {
        linkvisitor.Reset();
        _veclinks.at(ilink).first->accept(linkvisitor);
        Transform tlocal;
        if( linkvisitor.IsDone() ) {
            // have to do this since there can be draggers affecting _veclinks.at(ilink).first
            tlocal = GetRaveTransformFromMatrix(linkvisitor.wcMatrix).inverse() * tglob.inverse() * _vtrans.at(ilink);
        }
        else {
            tlocal = tglob.inverse() * _vtrans.at(ilink);
        }
        SetMatrixTransform(*_veclinks.at(ilink).second, tlocal);
    }

    return true;
}

void KinBodyItem::SetGrab(bool bGrab, bool bUpdate)
{
    if(!_pbody ) {
        return;
    }

    bGrabbed = bGrab;

    if( bUpdate ) {
        if( bGrab ) {
            UpdateFromModel();
        }
        else {
            UpdateFromOSG();
        }
    }
}

KinBody::LinkPtr KinBodyItem::GetLinkFromOSG(OSGNodePtr plinknode) const
{
    FindNode search(plinknode);
    for(size_t ilink = 0; ilink < _veclinks.size(); ++ilink) {
        search.apply(*_veclinks[ilink].second);
        if( search.IsFound() ) {
            return _pbody->GetLinks().at(ilink);
        }
    }

    return KinBody::LinkPtr();
}

RobotItem::RobotItem(OSGGroupPtr osgSceneRoot, RobotBasePtr robot, ViewGeometry viewgeom) : KinBodyItem(osgSceneRoot, robot, viewgeom)
{
    int index = 0;
    FOREACHC(itmanip, robot->GetManipulators()) {

        if((*itmanip)->GetEndEffector()) {
            OSGSwitchPtr peeswitch = new osg::Switch();
            OSGGroupPtr peesep = new osg::Group();
            OSGMatrixTransformPtr ptrans = new osg::MatrixTransform();
            _vEndEffectors.push_back(EE(index, ptrans, peeswitch));

            _osgdata->addChild(peeswitch);
            peeswitch->addChild(ptrans);
            peeswitch->setAllChildrenOff();
            ptrans->addChild(peesep);

            // set a diffuse color
            {
                osg::StateSet* state = peesep->getOrCreateStateSet();
                osg::ref_ptr<osg::Material> mat = new osg::Material;

                mat->setDiffuse( osg::Material::FRONT, osg::Vec4f(1,0.5,0.5,1) );
                mat->setAmbient( osg::Material::FRONT, osg::Vec4f(1,0.5,0.5,1));

                state->setAttribute(mat.get());

                osg::Sphere* sphere = new osg::Sphere();
                osg::Geode* geode = new osg::Geode;
                sphere->setRadius(0.004f);
                osg::ShapeDrawable* sd = new osg::ShapeDrawable(sphere);
                geode->addDrawable(sd);
                peesep->addChild(geode);
            }

            // add some axes
            OSGGroupPtr paxes = new osg::Group();

            Vector colors[] = {Vector(0,0,1),Vector(0,1,0),Vector(1,0,0)};
            Vector rotations[] = {Vector(1,0,0,M_PI/2), Vector(1,0,0,0), Vector(0,0,1,-M_PI/2)};

            // add 3 cylinder+cone axes
            for(int i = 0; i < 3; ++i) {
                // set a diffuse color
                OSGGroupPtr psep = new osg::Group();

                // set a diffuse color
                osg::ref_ptr<osg::StateSet> state = psep->getOrCreateStateSet();
                osg::ref_ptr<osg::Material> mat = new osg::Material;
                mat->setDiffuse(osg::Material::FRONT, osg::Vec4f(colors[i].x, colors[i].y, colors[i].z,1.0));
                mat->setAmbient(osg::Material::FRONT, osg::Vec4f(colors[i].x, colors[i].y, colors[i].z,1.0));

                state->setAttribute( mat );

                osg::Matrix matrix;
                OSGMatrixTransformPtr protation = new osg::MatrixTransform();
                matrix.makeRotate(osg::Quat(rotations[i].x,rotations[i].y,rotations[i].z,rotations[i].w));
                protation->setMatrix(matrix);

                matrix.makeIdentity();
                OSGMatrixTransformPtr pcyltrans = new osg::MatrixTransform();
                matrix.makeTranslate(0.0f,0.02f,0.0f);
                pcyltrans->setMatrix(matrix);

                // make SoCylinder point towards z, not y
                osg::Cylinder* cy = new osg::Cylinder();
                cy->setRadius(0.002f);
                cy->setHeight(0.04f);
                osg::ref_ptr<osg::Geode> gcyl = new osg::Geode;
                osg::ref_ptr<osg::ShapeDrawable> sdcyl = new osg::ShapeDrawable(cy);
                gcyl->addDrawable(sdcyl.get());

                osg::Cone* cone = new osg::Cone();
                cone->setRadius(0.004f);
                cone->setHeight(0.02f);

                osg::ref_ptr<osg::Geode> gcone = new osg::Geode;
                osg::ref_ptr<osg::ShapeDrawable> sdcone = new osg::ShapeDrawable(cone);

                matrix.makeIdentity();
                OSGMatrixTransformPtr pconetrans = new osg::MatrixTransform();
                matrix.setTrans(osg::Vec3f(0,0.02f,0));
                pconetrans->setMatrix(matrix);

                psep->addChild(protation);
                psep->addChild(pcyltrans);
                psep->addChild(gcyl.get());
                psep->addChild(pconetrans);
                psep->addChild(gcone.get());
                paxes->addChild(psep);
            }

            peesep->addChild(paxes);

            // add text
            {
                OSGGroupPtr ptextsep = new osg::Group();
                osg::Geode* textGeode = new osg::Geode;
                peesep->addChild(ptextsep);

                osg::Matrix matrix;
                OSGMatrixTransformPtr ptrans = new osg::MatrixTransform();
                ptrans->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
                matrix.setTrans(osg::Vec3f(0.02f,0.02f,0.02f));
                ptextsep->addChild(ptrans);

                osg::ref_ptr<osgText::Text> text = new osgText::Text();

                //Set the screen alignment - always face the screen
                text->setAxisAlignment(osgText::Text::SCREEN);

                text->setColor(osg::Vec4(0,0,0,1));
                text->setFontResolution(18,18);

                text->setText(str(boost::format("EE%d")%index));
                textGeode->addDrawable(text);
                ptextsep->addChild(textGeode);
            }
        }

        ++index;
    }
}

void RobotItem::SetGrab(bool bGrab, bool bUpdate)
{
    if( !_probot ) {
        return;
    }
    if( bGrab ) {
        // turn off any controller commands if a robot
        if( !!_probot->GetController() ) {
            _probot->GetController()->SetPath(TrajectoryBaseConstPtr());
        }
    }

    FOREACH(itee, _vEndEffectors) {
        if( !!itee->_pswitch ) {
            itee->_pswitch->setAllChildrenOn();
        }
        else {
            itee->_pswitch->setAllChildrenOff();
        }
    }
    FOREACH(itee, _vAttachedSensors) {
        if( !!itee->_pswitch ) {
            itee->_pswitch->setAllChildrenOn();
        }
        else {
            itee->_pswitch->setAllChildrenOff();
        }
    }

    KinBodyItem::SetGrab(bGrab, bUpdate);
}

bool RobotItem::UpdateFromOSG()
{
    if( !KinBodyItem::UpdateFromOSG() ) {
        return false;
    }
    return true;
}

bool RobotItem::UpdateFromModel(const vector<dReal>& vjointvalues, const vector<Transform>& vtrans)
{
    if( !KinBodyItem::UpdateFromModel(vjointvalues,vtrans) ) {
        return false;
    }
    if( bGrabbed ) {
        // only updated when grabbing!
        RaveTransform<float> transInvRoot = GetRaveTransform(*_osgWorldTransform).inverse();

        FOREACH(itee, _vEndEffectors) {
            if( itee->_index >= 0 && itee->_index < (int)_probot->GetManipulators().size()) {
                RobotBase::ManipulatorConstPtr manip = _probot->GetManipulators().at(itee->_index);
                if( !!manip->GetEndEffector() ) {
                    RaveTransform<float> tgrasp = vtrans.at(manip->GetEndEffector()->GetIndex())*manip->GetLocalToolTransform();
                    SetMatrixTransform(*itee->_ptrans, transInvRoot * tgrasp);
                }
            }
        }

        FOREACH(itee, _vAttachedSensors) {
            if( itee->_index >= 0 && itee->_index < (int)_probot->GetAttachedSensors().size()) {
                RobotBase::AttachedSensorConstPtr sensor = _probot->GetAttachedSensors().at(itee->_index);
                if( !!sensor->GetAttachingLink() ) {
                    RaveTransform<float> tgrasp = vtrans.at(sensor->GetAttachingLink()->GetIndex())*sensor->GetRelativeTransform();
                    SetMatrixTransform(*itee->_ptrans, transInvRoot * tgrasp);
                }
            }
        }
    }

    return true;
}

}