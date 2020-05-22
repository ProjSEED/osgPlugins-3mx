#include <osg/Notify>
#include <osg/PagedLOD>
#include <osg/Texture2D>

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/fstream>
#include <osgDB/Registry>

#include <iostream>
#include <stdio.h>
#include <string.h>

#include <osgDB/ReadFile>

#include "CJsonObject.hpp"
#include "openctm.h"

static CTMuint CTMCALL _ctmIfstremRead(void * aBuf /*out buf*/, CTMuint aCount,
	void * aUserData /*stream*/)
{
	return (CTMuint)((std::ifstream*)aUserData)->read((char*)aBuf, aCount).gcount();
}

struct Resource3MXB
{
	std::string type;
	std::string textureId;
	osg::ref_ptr<osg::Geometry> geometry;
	osg::ref_ptr<osg::Texture2D> texture;
};

class ReaderWriter3MXB : public osgDB::ReaderWriter
{
public:

	ReaderWriter3MXB()
	{
		supportsExtension("3mxb", "3mxb format");
	}

	virtual const char* className() const { return "3mxb reader"; }

private:
	bool readResources(std::ifstream& inFile, neb::CJsonObject& oJsonResourcesArray, std::map<std::string, Resource3MXB>& mapResource3MXB) const
	{
		int lastBufferOffset = inFile.tellg();
		int resourcesNum = oJsonResourcesArray.GetArraySize();
		for (int i = 0; i < resourcesNum; ++i)
		{
			auto& oJsonResource = oJsonResourcesArray[i];
			std::string id;
			std::string format;
			Resource3MXB resource3MXB;
			int bufferSize = 0;
			
			oJsonResource.Get("id", id);
			oJsonResource.Get("type", resource3MXB.type);
			oJsonResource.Get("format", format);
			if (resource3MXB.type == "textureBuffer" && format == "jpg")
			{
				oJsonResource.Get("size", bufferSize);
				osg::Image* image = nullptr;
				if(bufferSize)
				{
					//Read in file
					std::vector<char> buffer(bufferSize);
					inFile.read(&buffer[0], bufferSize);
					if ((int)inFile.gcount() != bufferSize)
					{
						return false;
					}
					lastBufferOffset = inFile.tellg();

					//Get ReaderWriter from file extension
					osgDB::ReaderWriter *reader = osgDB::Registry::instance()->getReaderWriterForExtension(format);

					osgDB::ReaderWriter::ReadResult rr;
					if (reader) {
						//Convert data to istream
						std::stringstream inputStream;
						inputStream.write(&buffer[0], bufferSize);

						//Attempt to read the image
						//osg::ref_ptr<const osgDB::ReaderWriter::Options> options;
						rr = reader->readImage(inputStream/*, options.get()*/);
					}

					//Return result
					if (rr.validImage()) {
						image = rr.takeImage();
					}
				}

				resource3MXB.texture = new osg::Texture2D();
				resource3MXB.texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
				resource3MXB.texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
				resource3MXB.texture->setDataVariance(osg::Object::STATIC);
				resource3MXB.texture->setResizeNonPowerOfTwoHint(false);
				resource3MXB.texture->setUnRefImageDataAfterApply(true);
				resource3MXB.texture->setImage(image);
			}
			else if (resource3MXB.type == "geometryBuffer" && format == "ctm")
			{
				oJsonResource.Get("size", bufferSize);
				oJsonResource.Get("texture", resource3MXB.textureId);

				CTMimporter ctm;
				ctm.LoadCustom(_ctmIfstremRead, &inFile);
				if ((int)inFile.tellg() - lastBufferOffset != bufferSize)
				{
					return false;
				}
				lastBufferOffset = inFile.tellg();

				// to osg
				resource3MXB.geometry = new osg::Geometry;

				auto vertCount = ctm.GetInteger(CTM_VERTEX_COUNT);
				if (vertCount)
				{
					auto vertices = ctm.GetFloatArray(CTM_VERTICES);
					osg::Vec3Array* osgVertices = new osg::Vec3Array(vertCount);
					memcpy(&osgVertices->asVector()[0], vertices, vertCount * sizeof(float) * 3);
					resource3MXB.geometry->setVertexArray(osgVertices);
				}

				auto hasNormals = ctm.GetInteger(CTM_HAS_NORMALS);
				if ((CTM_TRUE == hasNormals) && vertCount)
				{
					auto normals = ctm.GetFloatArray(CTM_NORMALS);
					osg::Vec3Array* osgNormals = new osg::Vec3Array(vertCount);
					memcpy(&osgNormals->asVector()[0], normals, vertCount * sizeof(float) * 3);
					resource3MXB.geometry->setNormalArray(osgNormals, osg::Vec3Array::BIND_PER_VERTEX);
				}

				auto uvMapCount = ctm.GetInteger(CTM_UV_MAP_COUNT);
				if (uvMapCount && vertCount)
				{
					auto uvmaps = ctm.GetFloatArray(CTM_UV_MAP_1);
					osg::Vec2Array* osgUVmaps = new osg::Vec2Array(vertCount);
					memcpy(&osgUVmaps->asVector()[0], uvmaps, vertCount * sizeof(float) * 2);
					resource3MXB.geometry->setTexCoordArray(0, osgUVmaps, osg::Vec2Array::BIND_PER_VERTEX);
				}

				auto triCount = ctm.GetInteger(CTM_TRIANGLE_COUNT);
				if (triCount)
				{
					auto indices = ctm.GetIntegerArray(CTM_INDICES);
					osg::DrawElements* osgPrimitives = new osg::DrawElementsUInt(GL_TRIANGLES, triCount * 3, indices);
					resource3MXB.geometry->addPrimitiveSet(osgPrimitives);
				}
			}
			else
			{
				return false;
			}
			mapResource3MXB.emplace(id, resource3MXB);
		}
		return true;
	}

public:
	virtual ReadResult readNode(const std::string& file, const osgDB::ReaderWriter::Options* options) const
	{
		std::string ext = osgDB::getLowerCaseFileExtension(file);
		if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

		std::string fileName = osgDB::findDataFile(file, options);
		if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

		OSG_INFO << "Reading file " << fileName << std::endl;

		std::ifstream inFile(fileName, std::ios::in | std::ios::binary);
		if (!inFile) {
			OSG_FATAL << "Reading file " << fileName << " failed! Can NOT open file." << std::endl;
			return ReadResult::ERROR_IN_READING_FILE;
		}
		
		// read magic number
		{
			const int magicNumberLen = 5;
			std::string magicNumber(magicNumberLen, '\0');
			inFile.read(&magicNumber[0], magicNumberLen);
			if (inFile.gcount() != magicNumberLen || magicNumber != "3MXBO")
			{
				OSG_FATAL << "Reading file " << fileName << " failed! Invalid magic number." << std::endl;
				return ReadResult::ERROR_IN_READING_FILE;
			}
		}

		// read header
		neb::CJsonObject oJson;
		{
			// read header size
			const int headerSizeLen = 4;
			uint32_t headerSize = 0;
			inFile.read((char*)&headerSize, headerSizeLen);
			if (inFile.gcount() != headerSizeLen)
			{
				OSG_FATAL << "Reading file " << fileName << " failed! Invalid header size." << std::endl;
				return ReadResult::ERROR_IN_READING_FILE;
			}

			// read header
			std::string header(headerSize, '\0');
			inFile.read(&header[0], headerSize);

			// parse header
			if (inFile.gcount() != headerSize || !oJson.Parse(header))
			{
				OSG_FATAL << "Reading file " << fileName << " failed! Invalid header." << std::endl;
				return ReadResult::ERROR_IN_READING_FILE;
			}
		}

		// version
		{
			int version = 1;
			oJson.Get("version", version);
			if (version != 1)
			{
				OSG_FATAL << "Reading file " << fileName << " failed! Un-supported version, only support version 1." << std::endl;
				return ReadResult::ERROR_IN_READING_FILE;
			}
		}

		// resources
		std::map<std::string, Resource3MXB> mapResource3MXB;
		if (!readResources(inFile, oJson["resources"], mapResource3MXB))
		{
			OSG_FATAL << "Reading file " << fileName << " failed! Invalid resources." << std::endl;
			return ReadResult::ERROR_IN_READING_FILE;
		}

		// nodes
		int nodesNum = oJson["nodes"].GetArraySize();
		osg::ref_ptr<osg::Group> group = new osg::Group;
		for (int i = 0; i < nodesNum; ++i)
		{
			float maxScreenDiameter = 0.f;
			std::string id;
			oJson["nodes"][i].Get("id", id);
			oJson["nodes"][i].Get("maxScreenDiameter", maxScreenDiameter);
			osg::Vec3 bbMin, bbMax;
			for (int j = 0; j < 3; ++j)
			{
				oJson["nodes"][i]["bbMin"].Get(j, bbMin[j]);
				oJson["nodes"][i]["bbMax"].Get(j, bbMax[j]);
			}

			osg::ref_ptr<osg::Geode> geode = new osg::Geode;
			int nodeResourcesNum = oJson["nodes"][i]["resources"].GetArraySize();
			for (int j = 0; j < nodeResourcesNum; ++j)
			{
				std::string resourceId;
				oJson["nodes"][i]["resources"].Get(j, resourceId);
				auto& resource3MXB = mapResource3MXB[resourceId];
				if (resource3MXB.type == "geometryBuffer")
				{
					geode->addDrawable(resource3MXB.geometry);
					if (resource3MXB.textureId.size())
					{
						resource3MXB.geometry->getOrCreateStateSet()->setTextureAttributeAndModes(0, mapResource3MXB[resource3MXB.textureId].texture, osg::StateAttribute::ON);
					}
				}
				else
				{

				}
			}

			int childNum = oJson["nodes"][i]["children"].GetArraySize();

			osg::ref_ptr<osg::PagedLOD> pagedLOD = new osg::PagedLOD;
			pagedLOD->setName(id);
			pagedLOD->setRangeMode(osg::PagedLOD::PIXEL_SIZE_ON_SCREEN);
			pagedLOD->setCenterMode(osg::PagedLOD::USER_DEFINED_CENTER);
			pagedLOD->setCenter((bbMin + bbMax) / 2.0f);
			pagedLOD->setRadius((bbMax - bbMin).length());
			if (childNum)
			{
				pagedLOD->addChild(geode, 0, maxScreenDiameter);
			}
			else
			{
				pagedLOD->addChild(geode, 0, 1e30);
			}

			// children
			for (int j = 0; j < childNum; ++j)
			{
				std::string childPath;
				oJson["nodes"][i]["children"].Get(j, childPath);
				std::string childName = osgDB::getFilePath(fileName) + "/" + childPath;
				pagedLOD->setFileName(j + 1, childName);
				pagedLOD->setRange(j + 1, maxScreenDiameter, 1e30);
			}

			// add to group
			group->addChild(pagedLOD);
		}

		group->setName(osgDB::getNameLessExtension(fileName));
		return group;
	}
};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(3mxb, ReaderWriter3MXB)