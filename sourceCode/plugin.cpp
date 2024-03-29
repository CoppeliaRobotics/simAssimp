#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <filesystem>
#include <simMath/7Vector.h>
#include <simStack/stackArray.h>
#include <simStack/stackMap.h>
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include <simPlusPlus/Plugin.h>
#include "config.h"
#include "plugin.h"
#include "stubs.h"

int parseVectorUp(int vu, int def)
{
    switch(vu)
    {
    case simassimp_upvect_auto:
        return 0;
    case simassimp_upvect_z:
        return 1;
    case simassimp_upvect_y:
        return 2;
    default:
        return def;
    }
}

void splitString(const std::string& str,char delChar,std::vector<std::string>& words)
{
    std::stringstream ss(str);
    std::string itm;
    while (std::getline(ss,itm,delChar))
        words.push_back(itm);
}

SIM_DLLEXPORT void simAssimp_getImportFormat(getImportFormat_in *in, getImportFormat_out *out)
{
    if(in->index < 0) throw std::runtime_error("invalid index");

    Assimp::Importer importer;
    if (in->index<importer.GetImporterCount())
    {
        out->formatDescription=importer.GetImporterInfo(in->index)->mName;
        out->formatExtension=importer.GetImporterInfo(in->index)->mFileExtensions;
    }
    else
        simPopStackItem(in->_.stackID,simGetStackSize(in->_.stackID));
}

SIM_DLLEXPORT void simAssimp_getExportFormat(getExportFormat_in *in, getExportFormat_out *out)
{
    if(in->index < 0) throw std::runtime_error("invalid index");

    Assimp::Exporter exporter;
    if (in->index<exporter.GetExportFormatCount())
    {
        out->formatDescription=exporter.GetExportFormatDescription(in->index)->description;
        out->formatExtension=exporter.GetExportFormatDescription(in->index)->fileExtension;
        out->formatId=exporter.GetExportFormatDescription(in->index)->id;
    }
    else
        simPopStackItem(in->_.stackID,simGetStackSize(in->_.stackID));
}

const aiMatrix4x4* getTransform(aiNode* node,const aiMatrix4x4* tr,int meshIndex)
{
    for (size_t i=0;i<node->mNumMeshes;i++)
    {
        if (node->mMeshes[i]==meshIndex)
            return(tr);
    }
    for (size_t i=0;i<node->mNumChildren;i++)
    {
        aiNode* childNode=node->mChildren[i];
        aiMatrix4x4 tr2=tr[0]*childNode->mTransformation;
        const aiMatrix4x4* m=getTransform(childNode,&tr2,meshIndex);
        if (m!=nullptr)
            return(m);
    }
    return(nullptr);
}

void assimpImportShapes(const char* fileNames,int maxTextures,double scaling,int upVector,int options,std::vector<int>& shapeHandles)
{
    std::vector<std::string> filenames;
    splitString(fileNames,';',filenames);
    for (size_t wi=0;wi<filenames.size();wi++)
    {
        std::string shapeAlias(filenames[wi]);
        std::size_t si=shapeAlias.find_last_of("/\\");
        if (si!=std::string::npos)
            shapeAlias=shapeAlias.substr(si+1);
        si=shapeAlias.find_last_of(".");
        if (si!=std::string::npos)
            shapeAlias=shapeAlias.substr(0,si);
        
        if ((options&256)==0)
        {
            std::string txt("importing ");
            txt+=filenames[wi];
            simAddLog("Assimp",sim_verbosity_infos,txt.c_str());
        }
        std::vector<int> shapeHandlesForThisFile;
        bool hasMaterials=false;
        Assimp::Importer importer;
        importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,aiComponent_ANIMATIONS|aiComponent_LIGHTS|aiComponent_CAMERAS);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,aiPrimitiveType_POINT|aiPrimitiveType_LINE);
        int flags=aiProcess_Triangulate|aiProcess_OptimizeGraph|
                aiProcess_SortByPType|aiProcess_RemoveComponent|aiProcess_DropNormals|
                aiProcess_RemoveRedundantMaterials|aiProcess_FindDegenerates|aiProcess_FindInvalidData|
                aiProcess_GenUVCoords|aiProcess_TransformUVCoords|aiProcess_EmbedTextures;

        if ((options&32)!=0)
            options=(options|24)-24;
        if ((options&8)==0)
            flags|=aiProcess_OptimizeMeshes;
        if ((options&16)==0)
            flags|=aiProcess_JoinIdenticalVertices;

        if ((options&128)!=0)
            importer.SetPropertyInteger(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION,1);
        const aiScene* scene = importer.ReadFile(filenames[wi].c_str(),flags);
        if(scene)
        {
            double minMaxX[2]={9999999.0,-9999999.0};
            double minMaxY[2]={9999999.0,-9999999.0};
            double minMaxZ[2]={9999999.0,-9999999.0};
            for (size_t i=0;i<scene->mNumMeshes;i++)
            {
                const aiMatrix4x4* tr=getTransform(scene->mRootNode,&scene->mRootNode->mTransformation,i);
                const aiMesh* mesh = scene->mMeshes[i];
                for (size_t j=0;j<mesh->mNumVertices;j++)
                {
                    if (tr!=nullptr)
                        mesh->mVertices[j]*=tr[0];
                    if (mesh->mVertices[j].x<minMaxX[0])
                        minMaxX[0]=mesh->mVertices[j].x;
                    if (mesh->mVertices[j].x>minMaxX[1])
                        minMaxX[1]=mesh->mVertices[j].x;
                    if (mesh->mVertices[j].y<minMaxY[0])
                        minMaxY[0]=mesh->mVertices[j].y;
                    if (mesh->mVertices[j].y>minMaxY[1])
                        minMaxY[1]=mesh->mVertices[j].y;
                    if (mesh->mVertices[j].z<minMaxZ[0])
                        minMaxZ[0]=mesh->mVertices[j].z;
                    if (mesh->mVertices[j].z>minMaxZ[1])
                        minMaxZ[1]=mesh->mVertices[j].z;
                }
            }
            double l=std::max<double>(minMaxX[1]-minMaxX[0],std::max<double>(minMaxY[1]-minMaxY[0],minMaxZ[1]-minMaxZ[0]));
            if (scaling==0.0)
            {
                scaling=1.0;
                while (l>5.0)
                {
                    l*=0.1;
                    scaling*=0.1;
                }
                while (l<0.05)
                {
                    l*=10.0;
                    scaling*=10.0;
                }
            }
            if (upVector==0)
            {
                if (minMaxZ[0]>=minMaxY[0])
                    upVector=1;
                else
                    upVector=2;
            }
            struct STexture
            {
                int imgRes[2];
                bool releaseBuffer;
                unsigned char* image;
            };
            std::map<int,STexture> allTransformedTextures;
            for (size_t i=0;i<scene->mNumMeshes;i++)
            {
                const aiMesh* mesh = scene->mMeshes[i];
                const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
                std::vector<double> vertices;
                std::vector<int> indices;
                for (size_t j=0;j<mesh->mNumVertices;j++)
                {
                    if (upVector==1)
                    {
                        vertices.push_back(mesh->mVertices[j].x*scaling);
                        vertices.push_back(mesh->mVertices[j].y*scaling);
                        vertices.push_back(mesh->mVertices[j].z*scaling);
                    }
                    else
                    {
                        vertices.push_back(mesh->mVertices[j].x*scaling);
                        vertices.push_back(-mesh->mVertices[j].z*scaling);
                        vertices.push_back(mesh->mVertices[j].y*scaling);
                    }
                }
                for (size_t j=0;j<mesh->mNumFaces;j++)
                {
                    const aiFace& face=mesh->mFaces[j];
                    indices.push_back(face.mIndices[0]);
                    indices.push_back(face.mIndices[1]);
                    indices.push_back(face.mIndices[2]);
                }
                
                // Ok, we have vertices and indices ready. What about a texture?
                std::vector<float> _textureCoords;
                float* textureCoords=nullptr; 
                unsigned char* imgg=nullptr;
                int imggRes[2];
                aiString texPath;
                bool hasTexture=false;
                if ( ((options&1)==0)&&(mesh->HasTextureCoords(0))&&(aiReturn_SUCCESS==aiGetMaterialTexture(material,aiTextureType_DIFFUSE,0,&texPath)) )
                {
                    for (size_t j=0;j<indices.size();j++)
                    {
                        int index=indices[j];
                        const aiVector3D& textureVect=mesh->mTextureCoords[0][index];
                        _textureCoords.push_back(float(textureVect.x));
                        _textureCoords.push_back(float(textureVect.y));
                    }
                    std::string p=std::string(texPath.C_Str());
                    const aiTexture* texture=nullptr;
                    int index=-1;
                    if ( (p.size()>1)&&(p[0]=='*') )
                    {
                        p.erase(0,1);
                        index=std::stoi(p);
                        texture=scene->mTextures[index];
                    }
                    if (_textureCoords.size()>0)
                    {
                        if (texture!=nullptr)
                        {
                            unsigned char* img=nullptr;
                            int res[2];
                            bool deleteTexture=true;
                            if (texture->mHeight==0)
                            {
                                int l=(int)texture->mWidth;
                                img=(unsigned char*)simLoadImage(res,1,(char*)texture->pcData,(int*)(&l));
                            }
                            else
                            {
                                img=(unsigned char*)texture->pcData;
                                res[0]=texture->mWidth;
                                res[1]=texture->mHeight;
                                deleteTexture=false;
                            }
                            std::map<int,STexture>::iterator textIt=allTransformedTextures.find(index);
                            if (textIt!=allTransformedTextures.end())
                            {
                                if (deleteTexture)
                                    simReleaseBuffer((char*)img);
                            }
                            else
                            {
                                if ( (res[0]>maxTextures)||(res[1]>maxTextures) )
                                {
                                    int resOut[2]={std::min<int>(maxTextures,res[0]),std::min<int>(maxTextures,res[1])};
                                    unsigned char* imgOut=simGetScaledImage(img,res,resOut,1+2,NULL);
                                    if (deleteTexture)
                                        simReleaseBuffer((char*)img);
                                    img=imgOut;
                                    res[0]=resOut[0];
                                    res[1]=resOut[1];
                                }
                                STexture im;
                                im.imgRes[0]=res[0];
                                im.imgRes[1]=res[1];
                                im.releaseBuffer=deleteTexture;
                                im.image=img;
                                allTransformedTextures[index]=im;
                                textIt=allTransformedTextures.find(index);
                            }
                            hasTexture=true;
                            hasMaterials=true;
                            textureCoords=_textureCoords.data();
                            imgg=textIt->second.image;
                            imggRes[0]=textIt->second.imgRes[0];
                            imggRes[1]=textIt->second.imgRes[1];
                            //simApplyTexture(h,&textureCoords[0],textureCoords.size(),textIt->second.image,textIt->second.imgRes,16);
                        }
                        else
                        {
                            std::filesystem::path pp(filenames[wi]);
                            pp=pp.parent_path();
                            std::string loc[6]={pp.string()+"/"+p,pp.string()+"/textures/"+p,pp.string()+"/../"+p,pp.string()+"/../textures/"+p,pp.string()+"/../materials/"+p,pp.string()+"/../materials/textures/"+p};
                            std::string fn;
                            for (size_t fi=0;fi<6;fi++)
                            {
                                if (simDoesFileExist(loc[fi].c_str())==1)
                                    fn=loc[fi];
                            }
                                printf("Final File: %s\n",fn.c_str());
                            if (fn.size()>0)
                            {
                                int res[2];
                                unsigned char* img=(unsigned char*)simLoadImage(res,1,fn.c_str(),nullptr);
                                if ( (res[0]>maxTextures)||(res[1]>maxTextures) )
                                {
                                    int resOut[2]={std::min<int>(maxTextures,res[0]),std::min<int>(maxTextures,res[1])};
                                    unsigned char* imgOut=simGetScaledImage(img,res,resOut,1+2,NULL);
                                    simReleaseBuffer((char*)img);
                                    img=imgOut;
                                    res[0]=resOut[0];
                                    res[1]=resOut[1];
                                }
                                STexture im;
                                im.imgRes[0]=res[0];
                                im.imgRes[1]=res[1];
                                im.releaseBuffer=true;
                                im.image=img;
                                allTransformedTextures[0]=im; // only one texture with textures with specfic name?
                                std::map<int,STexture>::iterator textIt=allTransformedTextures.find(0);
                                hasTexture=true;
                                hasMaterials=true;
                                textureCoords=_textureCoords.data();
                                imgg=textIt->second.image;
                                imggRes[0]=textIt->second.imgRes[0];
                                imggRes[1]=textIt->second.imgRes[1];
                                //simApplyTexture(h,&textureCoords[0],textureCoords.size(),textIt->second.image,textIt->second.imgRes,16);
                            }
                        }
                    }
                }
                
                int h=simCreateShape(16,0,&vertices[0],vertices.size(),&indices[0],indices.size(),nullptr,textureCoords,imgg,imggRes);
                std::string sha(shapeAlias);
                if (scene->mNumMeshes>1)
                {
                    sha+="_";
                    sha+=std::to_string(i);
                }
                simSetObjectAlias(h,sha.c_str(),0);
                
                if ((options&64)!=0)
                {
                    double ident[7]={0.0,0.0,0.0,0.0,0.0,0.0,1.0};
                    simAlignShapeBB(h,ident);
                }

                aiColor3D colorA(0.499,0.499,0.499);
                aiColor3D colorD(0.499,0.499,0.499);
                aiColor3D colorS(0.0,0.0,0.0);
                aiColor3D colorE(0.0,0.0,0.0);
                if ((options&2)==0)
                {
                    material->Get(AI_MATKEY_COLOR_AMBIENT,colorA);
                    material->Get(AI_MATKEY_COLOR_DIFFUSE,colorD);
                    material->Get(AI_MATKEY_COLOR_SPECULAR,colorS);
                    material->Get(AI_MATKEY_COLOR_EMISSIVE,colorE);
                }
                double opacity=1.0;
                if ((options&4)==0)
                    material->Get(AI_MATKEY_OPACITY,opacity);
                float ca[3]={(float)colorA.r,(float)colorA.g,(float)colorA.b};
                float cd[3]={(float)colorD.r,(float)colorD.g,(float)colorD.b};
                float cs[3]={(float)colorS.r,(float)colorS.g,(float)colorS.b};
                float ce[3]={(float)colorE.r,(float)colorE.g,(float)colorE.b};
                if ( hasTexture&&(ca[0]==0.0f)&&(ca[1]==0.0f)&&(ca[2]==0.0f) )
                {
                    ca[0]=0.499f;
                    ca[1]=0.499f;
                    ca[2]=0.499f;
                }
                float cad[3]={std::max<float>(ca[0],cd[0]),std::max<float>(ca[1],cd[1]),std::max<float>(ca[2],cd[2])};
                simSetShapeColor(h,nullptr,sim_colorcomponent_ambient_diffuse,cad);
                simSetShapeColor(h,nullptr,sim_colorcomponent_specular,cs);
                simSetShapeColor(h,nullptr,sim_colorcomponent_emission,ce);
                if ( (ca[0]!=0.499f)||(ca[1]!=0.499f)||(ca[2]!=0.499f) )
                    hasMaterials=true;
                if ( (cd[0]!=0.499f)||(cd[1]!=0.499f)||(cd[2]!=0.499f) )
                    hasMaterials=true;
                if (opacity!=1.0)
                {
                    float tr=float(1.0-opacity);
                    simSetShapeColor(h,nullptr,sim_colorcomponent_transparency,&tr);
                }
                shapeHandlesForThisFile.push_back(h);
            }
            // Free textures that need freedom:
            std::map<int,STexture>::iterator textIt;
            for (textIt=allTransformedTextures.begin();textIt!=allTransformedTextures.end();textIt++)
            {
                if (textIt->second.releaseBuffer)
                    simReleaseBuffer((char*)textIt->second.image);
            }
            allTransformedTextures.clear();
        }
        if ( ((options&32)!=0)&&(shapeHandlesForThisFile.size()>1) )
        {
            int s=1;
            if (!hasMaterials)
                s=-1;
            int h=simGroupShapes(&shapeHandlesForThisFile[0],s*int(shapeHandlesForThisFile.size()));
            shapeHandles.push_back(h);
            if ((options&64)!=0)
                simReorientShapeBoundingBox(h,-1,0);
        }
        else
            shapeHandles.insert(shapeHandles.end(),shapeHandlesForThisFile.begin(),shapeHandlesForThisFile.end());
    }
    simSetObjectSel(shapeHandles.data(),int(shapeHandles.size()));
}

SIM_DLLEXPORT void simAssimp_importShapes(importShapes_in *in, importShapes_out *out)
{
    if(in->maxTextureSize < 8) throw std::runtime_error("invalid maxTextureSize");
    if(in->scaling < 0.0) throw std::runtime_error("invalid scaling");
    if(in->upVector < 0) throw std::runtime_error("invalid upVector");
    if(in->upVector > 2) throw std::runtime_error("invalid upVector");
    if(in->options < 0) throw std::runtime_error("invalid options");

    std::vector<int> handles;
    assimpImportShapes(in->filenames.c_str(),in->maxTextureSize,in->scaling,parseVectorUp(in->upVector,0),in->options,handles);
    out->shapeHandles.assign(handles.begin(),handles.end());
}

void assimpExportShapes(const std::vector<int>& shapeHandles,const char* filename,const char* format,double scaling,int upVector,int options)
{
    if ((options&256)==0)
    {
        std::string txt("exporting ");
        txt+=filename;
        simAddLog("Assimp",sim_verbosity_infos,txt.c_str());
    }

    struct SShape
    {
        double* vertices;
        int verticesSize;
        int* indices;
        int indicesSize;
        double* normals;
        double colorAD[3];
        double colorS[3];
        double colorE[3];
        int textureId;
        float* textureCoordinates;
    };
    struct STexture
    {
        int imgRes[2];
        unsigned char* image;
        int textureId;
        std::string filename;
    };
    std::map<int,STexture> allTextures;
    int texturesCnt=0;
    std::vector<SShape> allShapeComponents;
    std::string filenameNoExt(filename);
    size_t ll=filenameNoExt.find_last_of('.');
    if (ll!=std::string::npos)
        filenameNoExt.resize(ll);
    C7Vector firstTrInv;
    for (size_t shapeI=0;shapeI<shapeHandles.size();shapeI++)
    {
        int h=shapeHandles[shapeI];

        if (shapeI==0)
        {
            simGetObjectPosition(h,-1,firstTrInv.X.data);
            double q[4];
            simGetObjectQuaternion(h,-1,q);
            firstTrInv.Q=C4Vector(q[3],q[0],q[1],q[2]);
            firstTrInv.inverse();
        }
        C7Vector tr;
        simGetObjectPosition(h,-1,tr.X.data);
        double q[4];
        simGetObjectQuaternion(h,-1,q);
        tr.Q=C4Vector(q[3],q[0],q[1],q[2]);
        if ((options&512)!=0)
            tr=firstTrInv*tr;
        int visible;
        simGetObjectInt32Param(h,sim_objintparam_visible,&visible);
        if ( ((options&8)==0)||(visible!=0) )
        {
            int compoundIndex=0;
            SShapeVizInfo shapeInfo;
            int res=simGetShapeViz(h,compoundIndex++,&shapeInfo);
            while (res>0)
            {
                if ((shapeInfo.textureOptions&8)==0)
                { // component is not wireframe (we ignore wireframe components)
                    double* __vert=nullptr;
                    if ((options&5)==5)
                    { // we drop textures and normals
                        __vert=shapeInfo.vertices;
                    }
                    else
                    { // we keep normals and/or textures. We need to duplicate vertices:
                        __vert=(double*)simCreateBuffer(3*shapeInfo.indicesSize*sizeof(double));
                        for (int i=0;i<shapeInfo.indicesSize;i++)
                        {
                            __vert[3*i+0]=shapeInfo.vertices[3*shapeInfo.indices[i]+0];
                            __vert[3*i+1]=shapeInfo.vertices[3*shapeInfo.indices[i]+1];
                            __vert[3*i+2]=shapeInfo.vertices[3*shapeInfo.indices[i]+2];
                            shapeInfo.indices[i]=i;
                        }
                        simReleaseBuffer((char*)shapeInfo.vertices);
                        shapeInfo.verticesSize=3*shapeInfo.indicesSize;
                    }
                    SShape s;
                    s.vertices=__vert;
                    s.verticesSize=shapeInfo.verticesSize;
                    for (int i=0;i<s.verticesSize/3;i++)
                    { // correctly transform the vertices:
                        C3Vector v(s.vertices+3*i);
                        v=tr*v;
                        if (upVector==1)
                        {
                            s.vertices[3*i+0]=v(0)*scaling;
                            s.vertices[3*i+1]=v(1)*scaling;
                            s.vertices[3*i+2]=v(2)*scaling;
                        }
                        else
                        {
                            s.vertices[3*i+0]=v(0)*scaling;
                            s.vertices[3*i+1]=v(2)*scaling;
                            s.vertices[3*i+2]=-v(1)*scaling;
                        }
                    }
                    s.indices=shapeInfo.indices;
                    s.indicesSize=shapeInfo.indicesSize;

                    s.normals=shapeInfo.normals;
                    for (int i=0;i<s.indicesSize;i++)
                    { // correctly transform the normals:
                        C3Vector v(s.normals+3*i);
                        v=tr.Q*v;
                        s.normals[3*i+0]=v(0);
                        s.normals[3*i+1]=v(1);
                        s.normals[3*i+2]=v(2);
                    }
                    s.colorAD[0]=shapeInfo.colors[0];
                    s.colorAD[1]=shapeInfo.colors[1];
                    s.colorAD[2]=shapeInfo.colors[2];
                    s.colorS[0]=shapeInfo.colors[3];
                    s.colorS[1]=shapeInfo.colors[4];
                    s.colorS[2]=shapeInfo.colors[5];
                    s.colorE[0]=shapeInfo.colors[6];
                    s.colorE[1]=shapeInfo.colors[7];
                    s.colorE[2]=shapeInfo.colors[8];
                    if ( (shapeInfo.texture!=nullptr)&&((options&1)==0) )
                    {
                        s.textureId=shapeInfo.textureId;
                        std::map<int,STexture>::iterator textIt=allTextures.find(s.textureId);
                        if (textIt==allTextures.end())
                        {
                            STexture t;
                            t.image=(unsigned char*)shapeInfo.texture;
                            t.imgRes[0]=shapeInfo.textureRes[0];
                            t.imgRes[1]=shapeInfo.textureRes[1];
                            t.textureId=shapeInfo.textureId;
                            t.filename=filenameNoExt+std::string("_")+std::to_string(t.textureId)+std::string(".png");
                            simSaveImage(t.image,t.imgRes,1,t.filename.c_str(),-1,nullptr);
                            size_t ll1=t.filename.find_last_of('/');
                            size_t ll2=t.filename.find_last_of('\\');
                            if ( (ll1!=std::string::npos)||(ll2!=std::string::npos) )
                            {
                                if (ll1==std::string::npos)
                                    t.filename.erase(t.filename.begin(),t.filename.begin()+ll2+1);
                                else
                                {
                                    if (ll2==std::string::npos)
                                        t.filename.erase(t.filename.begin(),t.filename.begin()+ll1+1);
                                    else
                                        t.filename.erase(t.filename.begin(),t.filename.begin()+std::max<int>(ll1,ll2)+1);
                                }
                            }
                            allTextures[shapeInfo.textureId]=t;
                            texturesCnt++;
                        }
                    }
                    else
                        s.textureId=-1;
                    s.textureCoordinates=shapeInfo.textureCoords;
                    allShapeComponents.push_back(s);
                }
                res=simGetShapeViz(h,compoundIndex++,&shapeInfo);
            }
        }
    }
    if (allShapeComponents.size()==0)
    {
        if ((options&256)==0)
        {
            std::string txt("nothing to export");
            simAddLog("Assimp",sim_verbosity_errors,txt.c_str());
        }
        return;
    }

    aiScene scene;
    scene.mRootNode=new aiNode();
    scene.mNumMaterials=allShapeComponents.size();
    scene.mMaterials=new aiMaterial*[allShapeComponents.size()];
    scene.mNumMeshes=allShapeComponents.size();
    scene.mMeshes=new aiMesh*[allShapeComponents.size()];
    scene.mRootNode->mNumMeshes=allShapeComponents.size();
    scene.mRootNode->mMeshes=new unsigned int[allShapeComponents.size()];
    for (size_t shapeCompI=0;shapeCompI<allShapeComponents.size();shapeCompI++)
    {
        scene.mMaterials[shapeCompI]=new aiMaterial();
        scene.mMeshes[shapeCompI]=new aiMesh();
        scene.mMeshes[shapeCompI]->mMaterialIndex=shapeCompI;
        scene.mRootNode->mMeshes[shapeCompI]=shapeCompI;
        auto pMaterial=scene.mMaterials[shapeCompI];
        if ((options&2)==0)
        {
            aiColor3D colorAD(allShapeComponents[shapeCompI].colorAD[0],allShapeComponents[shapeCompI].colorAD[1],allShapeComponents[shapeCompI].colorAD[2]);
            pMaterial->AddProperty(&colorAD,3,AI_MATKEY_COLOR_AMBIENT);
            pMaterial->AddProperty(&colorAD,3,AI_MATKEY_COLOR_DIFFUSE);
            aiColor3D colorS(allShapeComponents[shapeCompI].colorS[0],allShapeComponents[shapeCompI].colorS[1],allShapeComponents[shapeCompI].colorS[2]);
            pMaterial->AddProperty(&colorS,3,AI_MATKEY_COLOR_SPECULAR);
            aiColor3D colorE(allShapeComponents[shapeCompI].colorE[0],allShapeComponents[shapeCompI].colorE[1],allShapeComponents[shapeCompI].colorE[2]);
            pMaterial->AddProperty(&colorE,3,AI_MATKEY_COLOR_EMISSIVE);
        }

        auto pMesh=scene.mMeshes[shapeCompI];

        pMesh->mVertices=new aiVector3D[allShapeComponents[shapeCompI].verticesSize/3];
        pMesh->mNumVertices=allShapeComponents[shapeCompI].verticesSize/3;
        for (int i=0;i<allShapeComponents[shapeCompI].verticesSize/3;i++)
        {
            double* v=allShapeComponents[shapeCompI].vertices;
            pMesh->mVertices[i]=aiVector3D(v[3*i+0],v[3*i+1],v[3*i+2]);
        }

        if ((options&4)==0)
        {
            pMesh->mNormals=new aiVector3D[allShapeComponents[shapeCompI].indicesSize];
    //        pMesh->mNumNormals=allShapeComponents[shapeCompI].indicesSize;
            for (int i=0;i<allShapeComponents[shapeCompI].indicesSize;i++)
            {
                double* n=allShapeComponents[shapeCompI].normals;
                pMesh->mNormals[i]=aiVector3D(n[3*i+0],n[3*i+1],n[3*i+2]);
            }
        }

        pMesh->mFaces=new aiFace[allShapeComponents[shapeCompI].indicesSize/3];
        pMesh->mNumFaces=allShapeComponents[shapeCompI].indicesSize/3;
        for (int i=0;i<allShapeComponents[shapeCompI].indicesSize/3;i++)
        {
            int* tri=allShapeComponents[shapeCompI].indices;
            aiFace& face=pMesh->mFaces[i];
            face.mIndices=new unsigned int[3];
            face.mNumIndices=3;
            face.mIndices[0]=tri[3*i+0];
            face.mIndices[1]=tri[3*i+1];
            face.mIndices[2]=tri[3*i+2];
        }

        if ( (allShapeComponents[shapeCompI].textureCoordinates!=nullptr)&&((options&1)==0) )
        {
            pMesh->mTextureCoords[0]=new aiVector3D[allShapeComponents[shapeCompI].verticesSize/3];
            pMesh->mNumUVComponents[0]=allShapeComponents[shapeCompI].verticesSize/3;
            for (int i=0;i<allShapeComponents[shapeCompI].indicesSize/3;i++)
            {
                int* tri=allShapeComponents[shapeCompI].indices;
                int index[3]={tri[3*i+0],tri[3*i+1],tri[3*i+2]};
                float* tCoords=allShapeComponents[shapeCompI].textureCoordinates;
                pMesh->mTextureCoords[0][index[0]]=aiVector3D((double)tCoords[6*i+0],(double)tCoords[6*i+1],0.0);
                pMesh->mTextureCoords[0][index[1]]=aiVector3D((double)tCoords[6*i+2],(double)tCoords[6*i+3],0.0);
                pMesh->mTextureCoords[0][index[2]]=aiVector3D((double)tCoords[6*i+4],(double)tCoords[6*i+5],0.0);
            }
            std::map<int,STexture>::iterator textIt=allTextures.find(allShapeComponents[shapeCompI].textureId);
            if (textIt!=allTextures.end())
            {
                aiString filePath(textIt->second.filename);
                pMaterial->AddProperty(&filePath,AI_MATKEY_TEXTURE_DIFFUSE(0));
            }
        }
        else
        {
            pMesh->mTextureCoords[0]=nullptr;
            pMesh->mNumUVComponents[0]=0;
        }
    }

    Assimp::Exporter exporter;
    exporter.Export(&scene,format,filename);

    // Release memory:
    for (size_t i=0;i<allShapeComponents.size();i++)
    {
        simReleaseBuffer((char*)allShapeComponents[i].vertices);
        simReleaseBuffer((char*)allShapeComponents[i].indices);
        simReleaseBuffer((char*)allShapeComponents[i].normals);
        simReleaseBuffer((char*)allShapeComponents[i].textureCoordinates);
    }
    std::map<int,STexture>::iterator textIt;
    for (textIt=allTextures.begin();textIt!=allTextures.end();textIt++)
        simReleaseBuffer((char*)textIt->second.image);
}


SIM_DLLEXPORT void simAssimp_exportShapes(exportShapes_in *in, exportShapes_out *out)
{
    Assimp::Exporter exp;
    int fi=0;
    bool formatOk=false;
    while (fi<exp.GetExportFormatCount())
    {
        if (in->formatId.compare(exp.GetExportFormatDescription(fi++)->id)==0)
        {
            formatOk=true;
            break;
        }
    }

    if (in->shapeHandles.size()<1) throw std::runtime_error("invalid shapeHandles");
    if(!formatOk) throw std::runtime_error("invalid format");
    if(in->scaling < 0.001) throw std::runtime_error("invalid scaling");
    if(in->upVector < 1) throw std::runtime_error("invalid upVector");
    if(in->upVector > 2) throw std::runtime_error("invalid upVector");
    if(in->options < 0) throw std::runtime_error("invalid options");

    assimpExportShapes(in->shapeHandles,in->filename.c_str(),in->formatId.c_str(),in->scaling,parseVectorUp(in->upVector,0),in->options);
}

void assimpImportMeshes(const char* fileNames,double scaling,int upVector,int options,std::vector<std::vector<double>>& allVertices,std::vector<std::vector<int>>& allIndices)
{
    std::vector<std::string> filenames;
    splitString(fileNames,';',filenames);
    for (size_t wi=0;wi<filenames.size();wi++)
    {
        bool newFile=true;
        if ((options&256)==0)
        {
            std::string txt("importing ");
            txt+=filenames[wi];
            simAddLog("Assimp",sim_verbosity_infos,txt.c_str());
        }
        Assimp::Importer importer;
        importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,aiComponent_ANIMATIONS|aiComponent_LIGHTS|aiComponent_CAMERAS);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,aiPrimitiveType_POINT|aiPrimitiveType_LINE);
        int flags=aiProcess_Triangulate|aiProcess_OptimizeGraph|
                aiProcess_SortByPType|aiProcess_RemoveComponent|aiProcess_DropNormals|
                aiProcess_RemoveRedundantMaterials|aiProcess_FindDegenerates|aiProcess_FindInvalidData|
                aiProcess_GenUVCoords|aiProcess_TransformUVCoords|aiProcess_EmbedTextures;
        if ((options&32)!=0)
            options=(options|24)-24;
        if ((options&8)==0)
            flags|=aiProcess_OptimizeMeshes;
        if ((options&16)==0)
            flags|=aiProcess_JoinIdenticalVertices;
        if ((options&128)!=0)
            importer.SetPropertyInteger(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION,1);
        const aiScene* scene = importer.ReadFile(filenames[wi].c_str(),flags);
        if(scene)
        {
            double minMaxX[2]={9999999.0,-9999999.0};
            double minMaxY[2]={9999999.0,-9999999.0};
            double minMaxZ[2]={9999999.0,-9999999.0};
            for (size_t i=0;i<scene->mNumMeshes;i++)
            {
                const aiMatrix4x4* tr=getTransform(scene->mRootNode,&scene->mRootNode->mTransformation,i);
                const aiMesh* mesh = scene->mMeshes[i];
                for (size_t j=0;j<mesh->mNumVertices;j++)
                {
                    if (tr!=nullptr)
                        mesh->mVertices[j]*=tr[0];
                    if (mesh->mVertices[j].x<minMaxX[0])
                        minMaxX[0]=mesh->mVertices[j].x;
                    if (mesh->mVertices[j].x>minMaxX[1])
                        minMaxX[1]=mesh->mVertices[j].x;
                    if (mesh->mVertices[j].y<minMaxY[0])
                        minMaxY[0]=mesh->mVertices[j].y;
                    if (mesh->mVertices[j].y>minMaxY[1])
                        minMaxY[1]=mesh->mVertices[j].y;
                    if (mesh->mVertices[j].z<minMaxZ[0])
                        minMaxZ[0]=mesh->mVertices[j].z;
                    if (mesh->mVertices[j].z>minMaxZ[1])
                        minMaxZ[1]=mesh->mVertices[j].z;
                }
            }
            double l=std::max<double>(minMaxX[1]-minMaxX[0],std::max<double>(minMaxY[1]-minMaxY[0],minMaxZ[1]-minMaxZ[0]));
            if (scaling==0.0)
            {
                scaling=1.0;
                while (l>5.0)
                {
                    l*=0.1;
                    scaling*=0.1;
                }
                while (l<0.05)
                {
                    l*=10.0;
                    scaling*=10.0;
                }
            }
            if (upVector==0)
            {
                if (minMaxZ[0]>=minMaxY[0])
                    upVector=1;
                else
                    upVector=2;
            }
            for (size_t i=0;i<scene->mNumMeshes;i++)
            {
                const aiMesh* mesh = scene->mMeshes[i];
                std::vector<double> vertices;
                std::vector<int> indices;
                for (size_t j=0;j<mesh->mNumVertices;j++)
                {
                    if (upVector==1)
                    {
                        vertices.push_back(mesh->mVertices[j].x*scaling);
                        vertices.push_back(mesh->mVertices[j].y*scaling);
                        vertices.push_back(mesh->mVertices[j].z*scaling);
                    }
                    else
                    {
                        vertices.push_back(mesh->mVertices[j].x*scaling);
                        vertices.push_back(-mesh->mVertices[j].z*scaling);
                        vertices.push_back(mesh->mVertices[j].y*scaling);
                    }
                }
                for (size_t j=0;j<mesh->mNumFaces;j++)
                {
                    const aiFace& face=mesh->mFaces[j];
                    indices.push_back(face.mIndices[0]);
                    indices.push_back(face.mIndices[1]);
                    indices.push_back(face.mIndices[2]);
                }
                if ( newFile||((options&32)==0) )
                {
                    allVertices.push_back(std::vector<double>());
                    allIndices.push_back(std::vector<int>());
                    allVertices[allVertices.size()-1].assign(vertices.begin(),vertices.end());
                    allIndices[allIndices.size()-1].assign(indices.begin(),indices.end());
                }
                else
                {
                    int off=allVertices[allVertices.size()-1].size()/3;
                    allVertices[allVertices.size()-1].insert(allVertices[allVertices.size()-1].end(),vertices.begin(),vertices.end());
                    for (size_t i=0;i<indices.size();i++)
                        allIndices[allIndices.size()-1].push_back(indices[i]+off);
                }
                newFile=false;
            }
        }
    }
}

SIM_DLLEXPORT void simAssimp_importMeshes(importMeshes_in *in, importMeshes_out *out)
{
    if(in->scaling < 0.0) throw std::runtime_error("invalid scaling");
    if(in->upVector < 0) throw std::runtime_error("invalid upVector");
    if(in->upVector > 2) throw std::runtime_error("invalid upVector");
    if(in->options < 0) throw std::runtime_error("invalid options");

    std::vector<std::vector<double>> allVertices;
    std::vector<std::vector<int>> allIndices;
    assimpImportMeshes(in->filenames.c_str(),in->scaling,parseVectorUp(in->upVector,0),in->options,allVertices,allIndices);

    simPopStackItem(in->_.stackID,simGetStackSize(in->_.stackID));

    simPushTableOntoStack(in->_.stackID);
    for (size_t i=0;i<allVertices.size();i++)
    {
        simPushInt32OntoStack(in->_.stackID,int(i+1));
        if (allVertices[i].size()>0)
            simPushDoubleTableOntoStack(in->_.stackID,&allVertices[i][0],int(allVertices[i].size()));
        else
            simPushDoubleTableOntoStack(in->_.stackID,nullptr,0);
        simInsertDataIntoStackTable(in->_.stackID);
    }

    simPushTableOntoStack(in->_.stackID);
    for (size_t i=0;i<allIndices.size();i++)
    {
        simPushInt32OntoStack(in->_.stackID,int(i+1));
        if (allIndices[i].size()>0)
            simPushInt32TableOntoStack(in->_.stackID,&allIndices[i][0],int(allIndices[i].size()));
        else
            simPushInt32TableOntoStack(in->_.stackID,nullptr,0);
        simInsertDataIntoStackTable(in->_.stackID);
    }
}

void assimpExportMeshes(const std::vector<std::vector<double>>& vertices,const std::vector<std::vector<int>>& indices,const char* filename,const char* format,double scaling,int upVector,int options)
{
    if ((options&256)==0)
    {
        std::string txt("exporting ");
        txt+=filename;
        simAddLog("Assimp",sim_verbosity_infos,txt.c_str());
    }

    aiScene scene;
    scene.mRootNode=new aiNode();
    scene.mNumMaterials=vertices.size();
    scene.mMaterials=new aiMaterial*[vertices.size()];
    scene.mNumMeshes=vertices.size();
    scene.mMeshes=new aiMesh*[vertices.size()];
    scene.mRootNode->mNumMeshes=vertices.size();
    scene.mRootNode->mMeshes=new unsigned int[vertices.size()];
    for (size_t shapeCompI=0;shapeCompI<vertices.size();shapeCompI++)
    {
        scene.mMaterials[shapeCompI]=new aiMaterial();
        scene.mMeshes[shapeCompI]=new aiMesh();
        scene.mMeshes[shapeCompI]->mMaterialIndex=shapeCompI;
        scene.mRootNode->mMeshes[shapeCompI]=shapeCompI;

        auto pMesh=scene.mMeshes[shapeCompI];

        pMesh->mVertices=new aiVector3D[vertices[shapeCompI].size()/3];
        pMesh->mNumVertices=vertices[shapeCompI].size()/3;
        for (int i=0;i<vertices[shapeCompI].size()/3;i++)
        {
            const double* v=&(vertices[shapeCompI])[0];
            if (upVector!=2)
                pMesh->mVertices[i]=aiVector3D(v[3*i+0]*scaling,v[3*i+1]*scaling,v[3*i+2]*scaling);
            else
                pMesh->mVertices[i]=aiVector3D(v[3*i+0]*scaling,v[3*i+2]*scaling,-v[3*i+1]*scaling);
        }

        pMesh->mNormals=nullptr;

        pMesh->mFaces=new aiFace[indices[shapeCompI].size()/3];
        pMesh->mNumFaces=indices[shapeCompI].size()/3;
        for (int i=0;i<indices[shapeCompI].size()/3;i++)
        {
            const int* tri=&(indices[shapeCompI])[0];
            aiFace& face=pMesh->mFaces[i];
            face.mIndices=new unsigned int[3];
            face.mNumIndices=3;
            face.mIndices[0]=tri[3*i+0];
            face.mIndices[1]=tri[3*i+1];
            face.mIndices[2]=tri[3*i+2];
        }

        pMesh->mTextureCoords[0]=nullptr;
        pMesh->mNumUVComponents[0]=0;
    }

    Assimp::Exporter exporter;
    exporter.Export(&scene,format,filename);
}

#define LUA_EXPORTMESHES_COMMAND "simAssimp.exportMeshes"
SIM_DLLEXPORT void simAssimp_exportMeshes(exportMeshes_in *in, exportMeshes_out *out)
{
    int stack=in->_.stackID;
    CStackArray inArguments;
    inArguments.buildFromStack(stack);

    if (inArguments.getSize()>=4)
    {
        CStackArray* allVerticesA;
        CStackArray* allIndicesA;
        if (inArguments.isArray(0)&&inArguments.isArray(1))
        {
            allVerticesA=inArguments.getArray(0);
            allIndicesA=inArguments.getArray(1);
            if ( (allVerticesA->getSize()>0)&&(allVerticesA->getSize()==allIndicesA->getSize()) )
            {
                size_t l=allVerticesA->getSize();
                bool ok=true;
                for (size_t i=0;i<l;i++)
                {
                    if ( (!allVerticesA->isArray(i,1))||(!allIndicesA->isArray(i,1)) )
                    {
                        ok=false;
                        break;
                    }
                }
                if (ok)
                {
                    if (inArguments.isString(2))
                    {
                        std::string filename(inArguments.getString(2));
                        if (inArguments.isString(3))
                        {
                            std::string format(inArguments.getString(3));
                            double scaling=1.0;
                            int upVector=1;
                            int options=0;
                            if (inArguments.getSize()>4)
                            {
                                if ( (!inArguments.isNumber(4))||(inArguments.getDouble(4)<=0.0) )
                                {
                                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 5: invalid argument.");
                                    ok=false;
                                }
                                scaling = inArguments.getDouble(4);
                            }
                            if ( ok&&(inArguments.getSize()>5) )
                            {
                                if ( (!inArguments.isNumber(5))||(inArguments.getInt(5)<1)||(inArguments.getInt(5)>2) )
                                {
                                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 6: invalid argument.");
                                    ok=false;
                                }
                                upVector = inArguments.getInt(5);
                            }
                            if ( ok&&(inArguments.getSize()>6) )
                            {
                                if ( (!inArguments.isNumber(6))||(inArguments.getInt(6)!=0) )
                                {
                                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 7: invalid argument.");
                                    ok=false;
                                }
                                options = inArguments.getInt(6);
                            }
                            if (ok)
                            {
                                std::vector<std::vector<double>> allVertices;
                                std::vector<std::vector<int>> allIndices;
                                for (size_t i=0;i<l;i++)
                                {
                                    CStackArray* vertA=allVerticesA->getArray(i);
                                    CStackArray* indA=allIndicesA->getArray(i);
                                    const std::vector<double>* d=vertA->getDoubles();
                                    allVertices.push_back(std::vector<double>());
                                    for (size_t j=0;j<d->size();j++)
                                        allVertices[allVertices.size()-1].push_back(double(d[0][j]));
                                    allIndices.push_back(std::vector<int>(indA->getIntPointer(),indA->getIntPointer()+indA->getSize()));
                                }
                                assimpExportMeshes(allVertices,allIndices,filename.c_str(),format.c_str(),scaling,upVector,options);
                            }
                        }
                        else
                            simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 4: expected string.");
                    }
                    else
                        simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 3: expected string.");
                }
                else
                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 1&2: expected tables of tables.");
            }
            else
                simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 1&2: expected non-empty tables.");
        }
        else
            simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 1&2: expected tables.");
    }
    else
        simSetLastError(LUA_EXPORTMESHES_COMMAND,"Not enough arguments.");
}

SIM_DLLEXPORT int* assimp_importShapes(const char* fileNames,int maxTextures,double scaling,int upVector,int options,int* shapeCount)
{
    int* retVal=nullptr;
    std::vector<int> shapeHandles;
    assimpImportShapes(fileNames,maxTextures,scaling,upVector,options,shapeHandles);
    shapeCount[0]=int(shapeHandles.size());
    if (shapeHandles.size()>0)
    {
        retVal=(int*)simCreateBuffer(int(shapeHandles.size())*sizeof(int));
        for (size_t i=0;i<shapeHandles.size();i++)
            retVal[i]=shapeHandles[i];
    }
    return(retVal);
}

SIM_DLLEXPORT void assimp_exportShapes(const int* shapeHandles,int shapeCount,const char* filename,const char* format,double scaling,int upVector,int options)
{
    std::vector<int> handles(shapeHandles,shapeHandles+shapeCount);
    assimpExportShapes(handles,filename,format,scaling,upVector,options);
}

SIM_DLLEXPORT int assimp_importMeshes(const char* fileNames,double scaling,int upVector,int options,double*** allVertices,int** verticesSizes,int*** allIndices,int** indicesSizes)
{
    std::vector<std::vector<double>> _allVertices;
    std::vector<std::vector<int>> _allIndices;
    assimpImportMeshes(fileNames,scaling,upVector,options,_allVertices,_allIndices);
    int retVal=int(_allVertices.size());
    allVertices[0]=(double**)simCreateBuffer(retVal*sizeof(double*));
    verticesSizes[0]=(int*)simCreateBuffer(retVal*sizeof(int));
    if (allIndices!=nullptr)
    {
        allIndices[0]=(int**)simCreateBuffer(retVal*sizeof(int*));
        indicesSizes[0]=(int*)simCreateBuffer(retVal*sizeof(int));
    }
    for (size_t i=0;i<retVal;i++)
    {
        allVertices[0][i]=(double*)simCreateBuffer(_allVertices[i].size()*sizeof(double));
        verticesSizes[0][i]=int(_allVertices[i].size());
        for (size_t j=0;j<_allVertices[i].size();j++)
            allVertices[0][i][j]=_allVertices[i][j];
        if (allIndices!=nullptr)
        {
            allIndices[0][i]=(int*)simCreateBuffer(_allIndices[i].size()*sizeof(int));
            indicesSizes[0][i]=int(_allIndices[i].size());
            for (size_t j=0;j<_allIndices[i].size();j++)
                allIndices[0][i][j]=_allIndices[i][j];
        }
    }
    return(retVal);
}

SIM_DLLEXPORT void assimp_exportMeshes(int meshCnt,const double** allVertices,const int* verticesSizes,const int** allIndices,const int* indicesSizes,const char* filename,const char* format,double scaling,int upVector,int options)
{
    std::vector<std::vector<double>> _allVertices;
    std::vector<std::vector<int>> _allIndices;
    for (int i=0;i<meshCnt;i++)
    {
        _allVertices.push_back(std::vector<double>(allVertices[i],allVertices[i]+verticesSizes[i]));
        _allIndices.push_back(std::vector<int>(allIndices[i],allIndices[i]+indicesSizes[i]));
    }
    assimpExportMeshes(_allVertices,_allIndices,filename,format,scaling,upVector,options);
}

class Plugin : public sim::Plugin
{
public:
    void onInit()
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        setExtVersion("Assimp-based CAD Import Plugin");
        setBuildDate(BUILD_DATE);
    }
};

SIM_PLUGIN(Plugin)
