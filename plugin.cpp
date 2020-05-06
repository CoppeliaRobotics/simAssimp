#include <string>
#include <sstream>
#include <map>
#include <vector>
#include "7Vector.h"
#include "stackArray.h"
#include "stackMap.h"
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/matrix4x4.h>
#include <assimp/vector3.h>
#include "simPlusPlus/Plugin.h"
#include "plugin.h"
#include "stubs.h"

void outputMsg(int msgType,const char* msg)
{
    int plugin_verbosity = sim_verbosity_default;
    simGetModuleInfo(PLUGIN_NAME,sim_moduleinfo_verbosity,nullptr,&plugin_verbosity);
    if (plugin_verbosity>=msgType)
        printf("%s\n",msg);
}

int parseVectorUp(int vu, int def)
{
    switch(vu)
    {
    case sim_assimp_upvect_auto:
        return 0;
    case sim_assimp_upvect_z:
        return 1;
    case sim_assimp_upvect_y:
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

void getImportFormat(SScriptCallBack *p, const char *cmd, getImportFormat_in *in, getImportFormat_out *out)
{
    if(in->index < 0) throw std::runtime_error("invalid index");

    Assimp::Importer importer;
    if (in->index<importer.GetImporterCount())
    {
        out->formatDescription=importer.GetImporterInfo(in->index)->mName;
        out->formatExtension=importer.GetImporterInfo(in->index)->mFileExtensions;
    }
    else
        simPopStackItem(p->stackID,simGetStackSize(p->stackID));
}

void getExportFormat(SScriptCallBack *p, const char *cmd, getExportFormat_in *in, getExportFormat_out *out)
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
        simPopStackItem(p->stackID,simGetStackSize(p->stackID));
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

void assimpImportShapes(const char* fileNames,int maxTextures,float scaling,int upVector,int options,std::vector<int>& shapeHandles)
{
    std::vector<std::string> filenames;
    splitString(fileNames,';',filenames);
    for (size_t wi=0;wi<filenames.size();wi++)
    {
        if ((options&256)==0)
        {
            char txt[1000];
            snprintf(txt,sizeof(txt),"simExtAssimp: info: importing '%s'",filenames[wi].c_str());
            outputMsg(sim_verbosity_infos,txt);
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
            float minMaxX[2]={9999999.0f,-9999999.0f};
            float minMaxY[2]={9999999.0f,-9999999.0f};
            float minMaxZ[2]={9999999.0f,-9999999.0f};
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
            float l=std::max<float>(minMaxX[1]-minMaxX[0],std::max<float>(minMaxY[1]-minMaxY[0],minMaxZ[1]-minMaxZ[0]));
            if (scaling==0.0f)
            {
                scaling=1.0f;
                while (l>5.0f)
                {
                    l*=0.1f;
                    scaling*=0.1f;
                }
                while (l<0.05f)
                {
                    l*=10.0f;
                    scaling*=10.0f;
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
                std::vector<float> vertices;
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
                int h;
                h=simCreateMeshShape(0,0,&vertices[0],vertices.size(),&indices[0],indices.size(),nullptr);
                if ((options&64)!=0)
                    simReorientShapeBoundingBox(h,-1,0);

                aiString texPath;
                bool hasTexture=false;
                if ( ((options&1)==0)&&(mesh->HasTextureCoords(0))&&(aiReturn_SUCCESS==aiGetMaterialTexture(material,aiTextureType_DIFFUSE,0,&texPath)) )
                {
                    std::vector<float> textureCoords;
                    for (size_t j=0;j<indices.size();j++)
                    {
                        int index=indices[j];
                        const aiVector3D& textureVect=mesh->mTextureCoords[0][index];
                        textureCoords.push_back(textureVect.x);
                        textureCoords.push_back(textureVect.y);
                    }
                    std::string p=std::string(texPath.C_Str());
                    p.erase(0,1);
                    int index=std::stoi(p);
                    const aiTexture* texture=scene->mTextures[index];
                    if ( (texture!=nullptr)&&(textureCoords.size()>0) )
                    {
                        int res[2];
                        unsigned char* img=nullptr;
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
                        simApplyTexture(h,&textureCoords[0],textureCoords.size(),textIt->second.image,textIt->second.imgRes,16);
                    }
                }
                aiColor3D colorA(0.499f,0.499f,0.499f);
                aiColor3D colorD(0.499f,0.499f,0.499f);
                aiColor3D colorS(0.0f,0.0f,0.0f);
                aiColor3D colorE(0.0f,0.0f,0.0f);
                if ((options&2)==0)
                {
                    material->Get(AI_MATKEY_COLOR_AMBIENT,colorA);
                    material->Get(AI_MATKEY_COLOR_DIFFUSE,colorD);
                    material->Get(AI_MATKEY_COLOR_SPECULAR,colorS);
                    material->Get(AI_MATKEY_COLOR_EMISSIVE,colorE);
                }
                float opacity=1.0f;
                if ((options&4)==0)
                    material->Get(AI_MATKEY_OPACITY,opacity);
                float ca[3]={colorA.r,colorA.g,colorA.b};
                float cd[3]={colorD.r,colorD.g,colorD.b};
                float cs[3]={colorS.r,colorS.g,colorS.b};
                float ce[3]={colorE.r,colorE.g,colorE.b};
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
                if (opacity!=1.0f)
                {
                    float tr=1.0f-opacity;
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
    simRemoveObjectFromSelection(sim_handle_all,-1);
    for (size_t i=0;i<shapeHandles.size();i++)
        simAddObjectToSelection(sim_handle_single,shapeHandles[i]);
}

void importShapes(SScriptCallBack *p, const char *cmd, importShapes_in *in, importShapes_out *out)
{
    if(in->maxTextureSize < 8) throw std::runtime_error("invalid maxTextureSize");
    if(in->scaling < 0.0f) throw std::runtime_error("invalid scaling");
    if(in->upVector < 0) throw std::runtime_error("invalid upVector");
    if(in->upVector > 2) throw std::runtime_error("invalid upVector");
    if(in->options < 0) throw std::runtime_error("invalid options");

    std::vector<int> handles;
    assimpImportShapes(in->filenames.c_str(),in->maxTextureSize,in->scaling,parseVectorUp(in->upVector,0),in->options,handles);
    out->shapeHandles.assign(handles.begin(),handles.end());
}

void assimpExportShapes(const std::vector<int>& shapeHandles,const char* filename,const char* format,float scaling,int upVector,int options)
{
    if ((options&256)==0)
    {
        char txt[1000];
        snprintf(txt,sizeof(txt),"simExtAssimp: info: exporting '%s'",filename);
        outputMsg(sim_verbosity_infos,txt);
    }

    struct SShape
    {
        float* vertices;
        int verticesSize;
        int* indices;
        int indicesSize;
        float* normals;
        float colorAD[3];
        float colorS[3];
        float colorE[3];
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
    for (size_t shapeI=0;shapeI<shapeHandles.size();shapeI++)
    {
        int h=shapeHandles[shapeI];
        int visible;
        simGetObjectInt32Parameter(h,sim_objintparam_visible,&visible);
        if ( ((options&8)==0)||(visible!=0) )
        {
            C7Vector tr;
            simGetObjectPosition(h,-1,tr.X.data);
            float q[4];
            simGetObjectQuaternion(h,-1,q);
            tr.Q=C4Vector(q[3],q[0],q[1],q[2]);
            int compoundIndex=0;
            SShapeVizInfo shapeInfo;
            int res=simGetShapeViz(h,compoundIndex++,&shapeInfo);
            while (res>0)
            {
                if ((shapeInfo.textureOptions&8)==0)
                { // component is not wireframe (we ignore wireframe components)
                    float* __vert=nullptr;
                    if ((options&5)==5)
                    { // we drop textures and normals
                        __vert=shapeInfo.vertices;
                    }
                    else
                    { // we keep normals and/or textures. We need to duplicate vertices:
                        __vert=(float*)simCreateBuffer(3*shapeInfo.indicesSize*sizeof(float));
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
            outputMsg(sim_verbosity_errors,"simExtAssimp plugin error: nothing to export");
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
            float* v=allShapeComponents[shapeCompI].vertices;
            pMesh->mVertices[i]=aiVector3D(v[3*i+0],v[3*i+1],v[3*i+2]);
        }

        if ((options&4)==0)
        {
            pMesh->mNormals=new aiVector3D[allShapeComponents[shapeCompI].indicesSize];
    //        pMesh->mNumNormals=allShapeComponents[shapeCompI].indicesSize;
            for (int i=0;i<allShapeComponents[shapeCompI].indicesSize;i++)
            {
                float* n=allShapeComponents[shapeCompI].normals;
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
                pMesh->mTextureCoords[0][index[0]]=aiVector3D(tCoords[6*i+0],tCoords[6*i+1],0.0f);
                pMesh->mTextureCoords[0][index[1]]=aiVector3D(tCoords[6*i+2],tCoords[6*i+3],0.0f);
                pMesh->mTextureCoords[0][index[2]]=aiVector3D(tCoords[6*i+4],tCoords[6*i+5],0.0f);
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


void exportShapes(SScriptCallBack *p, const char *cmd, exportShapes_in *in, exportShapes_out *out)
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
    if(in->scaling < 0.001f) throw std::runtime_error("invalid scaling");
    if(in->upVector < 1) throw std::runtime_error("invalid upVector");
    if(in->upVector > 2) throw std::runtime_error("invalid upVector");
    if(in->options < 0) throw std::runtime_error("invalid options");

    assimpExportShapes(in->shapeHandles,in->filename.c_str(),in->formatId.c_str(),in->scaling,parseVectorUp(in->upVector,0),in->options);
}

void assimpImportMeshes(const char* fileNames,float scaling,int upVector,int options,std::vector<std::vector<float>>& allVertices,std::vector<std::vector<int>>& allIndices)
{
    std::vector<std::string> filenames;
    splitString(fileNames,';',filenames);
    for (size_t wi=0;wi<filenames.size();wi++)
    {
        bool newFile=true;
        if ((options&256)==0)
        {
            char txt[1000];
            snprintf(txt,sizeof(txt),"simExtAssimp: info: importing '%s'",filenames[wi].c_str());
            outputMsg(sim_verbosity_infos,txt);
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
            float minMaxX[2]={9999999.0f,-9999999.0f};
            float minMaxY[2]={9999999.0f,-9999999.0f};
            float minMaxZ[2]={9999999.0f,-9999999.0f};
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
            float l=std::max<float>(minMaxX[1]-minMaxX[0],std::max<float>(minMaxY[1]-minMaxY[0],minMaxZ[1]-minMaxZ[0]));
            if (scaling==0.0f)
            {
                scaling=1.0f;
                while (l>5.0f)
                {
                    l*=0.1f;
                    scaling*=0.1f;
                }
                while (l<0.05f)
                {
                    l*=10.0f;
                    scaling*=10.0f;
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
                std::vector<float> vertices;
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
                    allVertices.push_back(std::vector<float>());
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

void importMeshes(SScriptCallBack *p, const char *cmd, importMeshes_in *in, importMeshes_out *out)
{
    if(in->scaling < 0.0f) throw std::runtime_error("invalid scaling");
    if(in->upVector < 0) throw std::runtime_error("invalid upVector");
    if(in->upVector > 2) throw std::runtime_error("invalid upVector");
    if(in->options < 0) throw std::runtime_error("invalid options");

    std::vector<std::vector<float>> allVertices;
    std::vector<std::vector<int>> allIndices;
    assimpImportMeshes(in->filenames.c_str(),in->scaling,parseVectorUp(in->upVector,0),in->options,allVertices,allIndices);

    simPopStackItem(p->stackID,simGetStackSize(p->stackID));

    simPushTableOntoStack(p->stackID);
    for (size_t i=0;i<allVertices.size();i++)
    {
        simPushInt32OntoStack(p->stackID,int(i+1));
        if (allVertices[i].size()>0)
            simPushFloatTableOntoStack(p->stackID,&allVertices[i][0],int(allVertices[i].size()));
        else
            simPushFloatTableOntoStack(p->stackID,nullptr,0);
        simInsertDataIntoStackTable(p->stackID);
    }
    
    simPushTableOntoStack(p->stackID);
    for (size_t i=0;i<allIndices.size();i++)
    {
        simPushInt32OntoStack(p->stackID,int(i+1));
        if (allIndices[i].size()>0)
            simPushInt32TableOntoStack(p->stackID,&allIndices[i][0],int(allIndices[i].size()));
        else
            simPushInt32TableOntoStack(p->stackID,nullptr,0);
        simInsertDataIntoStackTable(p->stackID);
    }
}

void assimpExportMeshes(const std::vector<std::vector<float>>& vertices,const std::vector<std::vector<int>>& indices,const char* filename,const char* format,float scaling,int upVector,int options)
{
    if ((options&256)==0)
    {
        char txt[1000];
        snprintf(txt,sizeof(txt),"simExtAssimp: info: exporting '%s'",filename);
        outputMsg(sim_verbosity_infos,txt);
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
            const float* v=&(vertices[shapeCompI])[0];
            pMesh->mVertices[i]=aiVector3D(v[3*i+0],v[3*i+1],v[3*i+2]);
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
void exportMeshes(SScriptCallBack *p, const char *cmd, exportMeshes_in *in, exportMeshes_out *out)
{
    int stack=p->stackID;
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
                            float scaling=1.0f;
                            int upVector=1;
                            int options=0;
                            if (inArguments.getSize()>4)
                            {
                                if ( (!inArguments.isNumber(4))||(inArguments.getFloat(4)<=0.0f) )
                                {
                                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 5: invalid argument.");
                                    ok=false;
                                }
                            }
                            if ( ok&&(inArguments.getSize()>5) )
                            {
                                if ( (!inArguments.isNumber(5))||(inArguments.getInt(5)<1)||(inArguments.getInt(5)>2) )
                                {
                                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 6: invalid argument.");
                                    ok=false;
                                }
                            }
                            if ( ok&&(inArguments.getSize()>6) )
                            {
                                if ( (!inArguments.isNumber(6))||(inArguments.getInt(6)!=0) )
                                {
                                    simSetLastError(LUA_EXPORTMESHES_COMMAND,"argument 7: invalid argument.");
                                    ok=false;
                                }
                            }
                            if (ok)
                            {
                                std::vector<std::vector<float>> allVertices;
                                std::vector<std::vector<int>> allIndices;
                                for (size_t i=0;i<l;i++)
                                {
                                    CStackArray* vertA=allVerticesA->getArray(i);
                                    CStackArray* indA=allIndicesA->getArray(i);
                                    const std::vector<double>* d=vertA->getDoubles();
                                    allVertices.push_back(std::vector<float>());
                                    for (size_t j=0;j<d->size();j++)
                                        allVertices[allVertices.size()-1].push_back(float(d[0][j]));
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

SIM_DLLEXPORT int* assimp_importShapes(const char* fileNames,int maxTextures,float scaling,int upVector,int options,int* shapeCount)
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

SIM_DLLEXPORT void assimp_exportShapes(const int* shapeHandles,int shapeCount,const char* filename,const char* format,float scaling,int upVector,int options)
{
    std::vector<int> handles(shapeHandles,shapeHandles+shapeCount);
    assimpExportShapes(handles,filename,format,scaling,upVector,options);
}

SIM_DLLEXPORT int assimp_importMeshes(const char* fileNames,float scaling,int upVector,int options,float*** allVertices,int** verticesSizes,int*** allIndices,int** indicesSizes)
{
    std::vector<std::vector<float>> _allVertices;
    std::vector<std::vector<int>> _allIndices;
    assimpImportMeshes(fileNames,scaling,upVector,options,_allVertices,_allIndices);
    int retVal=int(_allVertices.size());
    allVertices[0]=(float**)simCreateBuffer(retVal*sizeof(float*));
    verticesSizes[0]=(int*)simCreateBuffer(retVal*sizeof(int));
    if (allIndices!=nullptr)
    {
        allIndices[0]=(int**)simCreateBuffer(retVal*sizeof(int*));
        indicesSizes[0]=(int*)simCreateBuffer(retVal*sizeof(int));
    }
    for (size_t i=0;i<retVal;i++)
    {
        allVertices[0][i]=(float*)simCreateBuffer(_allVertices[i].size()*sizeof(float));
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

SIM_DLLEXPORT void assimp_exportMeshes(int meshCnt,const float** allVertices,const int* verticesSizes,const int** allIndices,const int* indicesSizes,const char* filename,const char* format,float scaling,int upVector,int options)
{
    std::vector<std::vector<float>> _allVertices;
    std::vector<std::vector<int>> _allIndices;
    for (int i=0;i<meshCnt;i++)
    {
        _allVertices.push_back(std::vector<float>(allVertices[i],allVertices[i]+verticesSizes[i]));
        _allIndices.push_back(std::vector<int>(allIndices[i],allIndices[i]+indicesSizes[i]));
    }
    assimpExportMeshes(_allVertices,_allIndices,filename,format,scaling,upVector,options);
}

class Plugin : public sim::Plugin
{
public:
    void onStart()
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        setExtVersion("Assimp-based CAD Import Plugin");
        setBuildDate(BUILD_DATE);
    }
};

SIM_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
