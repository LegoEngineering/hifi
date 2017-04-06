//
//  FBXBaker.cpp
//  libraries/model-baking/src
//
//  Created by Stephen Birarda on 3/30/17.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <fbxsdk.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <NetworkAccessManager.h>

#include "ModelBakingLoggingCategory.h"
#include "TextureBaker.h"

#include "FBXBaker.h"


FBXBaker::FBXBaker(QUrl fbxURL, QString baseOutputPath) :
    _fbxURL(fbxURL),
    _baseOutputPath(baseOutputPath)
{
    // create an FBX SDK manager
    _sdkManager = FbxManager::Create();

    // grab the name of the FBX from the URL, this is used for folder output names
    auto fileName = fbxURL.fileName();
    _fbxName = fileName.left(fileName.indexOf('.'));
}

FBXBaker::~FBXBaker() {
    _sdkManager->Destroy();
}

static const QString BAKED_OUTPUT_SUBFOLDER = "baked/";
static const QString RAW_OUTPUT_SUBFOLDER = "raw/";

QString FBXBaker::pathToCopyOfRaw() const {
    return _uniqueOutputPath + RAW_OUTPUT_SUBFOLDER + _fbxURL.fileName();
}

void FBXBaker::start() {
    qCDebug(model_baking) << "Baking" << _fbxURL;

    // setup the output folder for the results of this bake
    if (!setupOutputFolder()) {
        return;
    }

    // check if the FBX is local or first needs to be downloaded
    if (_fbxURL.isLocalFile()) {
        // load up the local file
        QFile localFBX { _fbxURL.toLocalFile() };

        // make a copy in the output folder
        localFBX.copy(pathToCopyOfRaw());

        // start the bake now that we have everything in place
        bake();
    } else {
        // remote file, kick off a download
        auto& networkAccessManager = NetworkAccessManager::getInstance();

        QNetworkRequest networkRequest;

        // setup the request to follow re-directs and always hit the network
        networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        networkRequest.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

        networkRequest.setUrl(_fbxURL);

        qCDebug(model_baking) << "Downloading" << _fbxURL;

        auto networkReply = networkAccessManager.get(networkRequest);
        connect(networkReply, &QNetworkReply::finished, this, &FBXBaker::handleFBXNetworkReply);
    }
}

bool FBXBaker::setupOutputFolder() {
    // construct the output path using the name of the fbx and the base output path
    _uniqueOutputPath =  _baseOutputPath + "/" + _fbxName + "/";

    // make sure there isn't already an output directory using the same name
    int iteration = 0;

    while (QDir(_uniqueOutputPath).exists()) {
        _uniqueOutputPath = _baseOutputPath + "/" + _fbxName + "-" + QString::number(++iteration) + "/";
    }

    qCDebug(model_baking) << "Creating FBX output folder" << _uniqueOutputPath;

    // attempt to make the output folder
    if (!QDir().mkdir(_uniqueOutputPath)) {
        qCCritical(model_baking) << "Failed to create FBX output folder" << _uniqueOutputPath;

        emit finished();
        return false;
    }

    // make the baked and raw sub-folders used during export
    QDir uniqueOutputDir = _uniqueOutputPath;
    if (!uniqueOutputDir.mkdir(BAKED_OUTPUT_SUBFOLDER) || !uniqueOutputDir.mkdir(RAW_OUTPUT_SUBFOLDER)) {
        qCCritical(model_baking) << "Failed to create baked/raw subfolders in" << _uniqueOutputPath;

        emit finished();
        return false;
    }

    return true;
}

void FBXBaker::handleFBXNetworkReply() {
    QNetworkReply* requestReply = qobject_cast<QNetworkReply*>(sender());

    if (requestReply->error() == QNetworkReply::NoError) {
        qCDebug(model_baking) << "Downloaded" << _fbxURL;

        // grab the contents of the reply and make a copy in the output folder
        QFile copyOfRaw(pathToCopyOfRaw());

        qDebug(model_baking) << "Writing copy of raw FBX to" << copyOfRaw.fileName();

        if (!copyOfRaw.open(QIODevice::WriteOnly) || (copyOfRaw.write(requestReply->readAll()) == -1)) {

            // add an error to the error list for this FBX stating that a duplicate of the raw FBX could not be made
            emit finished();
            return;
        }

        // close that file now that we are done writing to it
        copyOfRaw.close();

        // kick off the bake process now that everything is ready to go
        bake();
    } else {
        qDebug() << "ERROR DOWNLOADING FBX" << requestReply->errorString();
    }
}

void FBXBaker::bake() {
    // (1) load the scene from the FBX file
    // (2) enumerate the textures found in the scene and start a bake for them
    // (3) export the FBX with re-written texture references

    importScene();
    rewriteAndBakeSceneTextures();
    exportScene();
}

bool FBXBaker::importScene() {
    // create an FBX SDK importer
    FbxImporter* importer = FbxImporter::Create(_sdkManager, "");

    // import the copy of the raw FBX file
    QString rawCopyPath = pathToCopyOfRaw();
    bool importStatus = importer->Initialize(rawCopyPath.toLocal8Bit().data());

    if (!importStatus) {
        // failed to initialize importer, print an error and return
        qCCritical(model_baking) << "Failed to import FBX file at" << _fbxURL
            << "- error:" << importer->GetStatus().GetErrorString();

        return false;
    } else {
        qCDebug(model_baking) << "Imported" << _fbxURL << "to FbxScene";
    }

    // setup a new scene to hold the imported file
    _scene = FbxScene::Create(_sdkManager, "bakeScene");

    // import the file to the created scene
    importer->Import(_scene);

    // destroy the importer that is no longer needed
    importer->Destroy();

    return true;
}

static const QString BAKED_TEXTURE_DIRECTORY = "textures/";
static const QString BAKED_TEXTURE_EXT = ".ktx";

QString texturePathRelativeToFBX(QUrl fbxURL, QUrl textureURL) {
    auto fbxPath = fbxURL.toString(QUrl::RemoveFilename | QUrl::RemoveQuery | QUrl::RemoveFragment);
    auto texturePath = textureURL.toString(QUrl::RemoveFilename | QUrl::RemoveQuery | QUrl::RemoveFragment);

    if (texturePath.startsWith(fbxPath)) {
        // texture path is a child of the FBX path, return the texture path without the fbx path
        return texturePath.mid(fbxPath.length());
    } else {
        // the texture path was not a child of the FBX path, return the empty string
        return "";
    }
}

bool FBXBaker::rewriteAndBakeSceneTextures() {
    // get a count of the textures used in the scene
    int numTextures = _scene->GetTextureCount();

    // enumerate the textures in the scene
    for (int i = 0; i < numTextures; i++) {
        // grab each file texture
        FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(_scene->GetTexture(i));

        if (fileTexture) {
            // use QFileInfo to easily split up the existing texture filename into its components
            QFileInfo textureFileInfo { fileTexture->GetFileName() };

            // make sure this texture points to something
            if (!textureFileInfo.filePath().isEmpty()) {

                // construct the new baked texture file name and file path

                // first make sure we have a unique base name for this texture
                // in case another texture referenced by this model has the same base name
                auto& nameMatches = _textureNameMatchCount[textureFileInfo.baseName()];

                QString bakedTextureFileName { textureFileInfo.baseName() };

                if (nameMatches > 0) {
                    // there are already nameMatches texture with this name
                    // append - and that number to our baked texture file name so that it is unique
                    bakedTextureFileName += "-" + QString::number(nameMatches);
                }

                bakedTextureFileName += BAKED_TEXTURE_EXT;

                // increment the number of name matches
                ++nameMatches;

                QString bakedTextureFilePath { _uniqueOutputPath + BAKED_OUTPUT_SUBFOLDER + BAKED_TEXTURE_DIRECTORY + bakedTextureFileName };

                qCDebug(model_baking).noquote() << "Re-mapping" << fileTexture->GetFileName() << "to" << bakedTextureFilePath;

                // write the new filename into the FBX scene
                fileTexture->SetFileName(bakedTextureFilePath.toLocal8Bit());

                QUrl urlToTexture;

                // add the texture to the list of textures needing to be baked
                if (textureFileInfo.exists() && textureFileInfo.isFile()) {
                    // set the texture URL to the local texture that we have confirmed exists
                    urlToTexture = QUrl::fromLocalFile(textureFileInfo.absoluteFilePath());
                } else {
                    // external texture that we'll need to download or find

                    // first check if it the RelativePath to the texture in the FBX was relative
                    QString relativeFileName = fileTexture->GetRelativeFileName();
                    auto apparentRelativePath = QFileInfo(relativeFileName.replace("\\", "/"));

#ifndef Q_OS_WIN
                    // it turns out that paths that start with a drive letter and a colon appear to QFileInfo
                    // as a relative path on UNIX systems - we perform a special check here to handle that case
                    bool isAbsolute = relativeFileName[1] == ':' || apparentRelativePath.isAbsolute();
#else
                    bool isAbsolute = apparentRelativePath.isAbsolute();
#endif

                    if (isAbsolute) {
                        // this is a relative file path which will require different handling
                        // depending on the location of the original FBX
                        if (_fbxURL.isLocalFile()) {
                            // since the loaded FBX is loaded, first check if we actually have the texture locally
                            // at the absolute path
                            if (apparentRelativePath.exists() && apparentRelativePath.isFile()) {
                                // the absolute path we ran into for the texture in the FBX exists on this machine
                                // so use that file
                                urlToTexture = QUrl::fromLocalFile(apparentRelativePath.absoluteFilePath());
                            } else {
                                // we didn't find the texture on this machine at the absolute path
                                // so assume that it is right beside the FBX to match the behaviour of interface
                                urlToTexture = _fbxURL.resolved(apparentRelativePath.fileName());
                            }
                        } else {
                            // the original FBX was remote and downloaded

                            // since this "relative" texture path is actually absolute, we have to assume it is beside the FBX
                            // which matches the behaviour of Interface

                            // append that path to our list of unbaked textures
                            urlToTexture = _fbxURL.resolved(apparentRelativePath.fileName());
                        }
                    } else {
                        // simply construct a URL with the relative path to the asset, locally or remotely
                        // and append that to the list of unbaked textures
                        urlToTexture = _fbxURL.resolved(apparentRelativePath.filePath());
                    }
                }

                // add the deduced url to the texture, associated with the resulting baked texture file name, to our hash
                _unbakedTextures.insert(urlToTexture, bakedTextureFileName);

                // bake this texture asynchronously
                bakeTexture(urlToTexture);
            }
        }
    }

    return true;
}

void FBXBaker::bakeTexture(const QUrl& textureURL) {
    // start a bake for this texture and add it to our list to keep track of
    auto bakingTexture = new TextureBaker(textureURL);

    connect(bakingTexture, &TextureBaker::finished, this, &FBXBaker::handleBakedTexture);

    bakingTexture->start();

    _bakingTextures.emplace_back(bakingTexture);
}

void FBXBaker::handleBakedTexture() {
    auto bakedTexture = qobject_cast<TextureBaker*>(sender());

    // use the path to the texture being baked to determine if this was an embedded or a linked texture

    // it is embeddded if the texure being baked was inside the unbaked output folder
    //

    auto rawOutputFolder = QUrl::fromLocalFile(_uniqueOutputPath + RAW_OUTPUT_SUBFOLDER);

    if (!rawOutputFolder.isParentOf(bakedTexture->getTextureURL())) {
        // for linked textures we want to save a copy of original texture beside the original FBX

        qCDebug(model_baking) << "Saving raw texture for" << bakedTexture->getTextureURL();

        // check if we have a relative path to use for the texture
        auto relativeTexturePath = texturePathRelativeToFBX(_fbxURL, bakedTexture->getTextureURL());

        QFile originalTextureFile {
            _uniqueOutputPath + RAW_OUTPUT_SUBFOLDER + relativeTexturePath + bakedTexture->getTextureURL().fileName()
        };

        if (relativeTexturePath.length() > 0) {
            // make the folders needed by the relative path
        }

        if (originalTextureFile.open(QIODevice::WriteOnly) && originalTextureFile.write(bakedTexture->getOriginalTexture()) != -1) {
            qCDebug(model_baking) << "Saved original texture file" << originalTextureFile.fileName()
            << "for" << _fbxURL;
        } else {
            qCWarning(model_baking) << "Could not save original external texture" << originalTextureFile.fileName()
            << "for" << _fbxURL;
        }
    }
}

static const QString BAKED_FBX_EXTENSION = ".baked.fbx";

bool FBXBaker::exportScene() {
    // setup the exporter
    FbxExporter* exporter = FbxExporter::Create(_sdkManager, "");

    auto rewrittenFBXPath = _uniqueOutputPath + BAKED_OUTPUT_SUBFOLDER + _fbxName + BAKED_FBX_EXTENSION;
    bool exportStatus = exporter->Initialize(rewrittenFBXPath.toLocal8Bit().data());

    if (!exportStatus) {
        // failed to initialize exporter, print an error and return
         qCCritical(model_baking) << "Failed to export FBX file at" << _fbxURL
            << "to" << rewrittenFBXPath << "- error:" << exporter->GetStatus().GetErrorString();

        return false;
    }

    // export the scene
    exporter->Export(_scene);

    qCDebug(model_baking) << "Exported" << _fbxURL << "with re-written paths to" << rewrittenFBXPath;

    return true;
}


bool FBXBaker::removeEmbeddedMediaFolder() {
    // now that the bake is complete, remove the embedded media folder produced by the FBX SDK when it imports an FBX
    auto embeddedMediaFolderName = _fbxURL.fileName().replace(".fbx", ".fbm");
    QDir(_uniqueOutputPath + RAW_OUTPUT_SUBFOLDER + embeddedMediaFolderName).removeRecursively();

    // we always return true because a failure to delete the embedded media folder is not a failure of the bake
    return true;
}
