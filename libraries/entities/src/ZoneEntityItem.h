//
//  ZoneEntityItem.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ZoneEntityItem_h
#define hifi_ZoneEntityItem_h

#include "KeyLightPropertyGroup.h"
#include "EntityItem.h"
#include "EntityTree.h"
#include "SkyboxPropertyGroup.h"
#include "HazePropertyGroup.h"
#include "StagePropertyGroup.h"
#include <ComponentMode.h>

class ZoneEntityItem : public EntityItem {
public:
    static EntityItemPointer factory(const EntityItemID& entityID, const EntityItemProperties& properties);

    ZoneEntityItem(const EntityItemID& entityItemID);

    ALLOW_INSTANTIATION // This class can be instantiated

    // methods for getting/setting all properties of an entity
    virtual EntityItemProperties getProperties(EntityPropertyFlags desiredProperties = EntityPropertyFlags()) const override;
    virtual bool setProperties(const EntityItemProperties& properties) override;
    virtual bool setSubClassProperties(const EntityItemProperties& properties) override;

    // TODO: eventually only include properties changed since the params.nodeData->getLastTimeBagEmpty() time
    virtual EntityPropertyFlags getEntityProperties(EncodeBitstreamParams& params) const override;

    virtual void appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                    EntityTreeElementExtraEncodeDataPointer modelTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const override;

    virtual int readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged) override;



    static bool getZonesArePickable() { return _zonesArePickable; }
    static void setZonesArePickable(bool value) { _zonesArePickable = value; }

    static bool getDrawZoneBoundaries() { return _drawZoneBoundaries; }
    static void setDrawZoneBoundaries(bool value) { _drawZoneBoundaries = value; }

    virtual bool isReadyToComputeShape() const override { return false; }
    void setShapeType(ShapeType type) override { withWriteLock([&] { _shapeType = type; }); }
    virtual ShapeType getShapeType() const override;

    virtual bool hasCompoundShapeURL() const;
    QString getCompoundShapeURL() const;
    virtual void setCompoundShapeURL(const QString& url);

    KeyLightPropertyGroup getKeyLightProperties() const { return resultWithReadLock<KeyLightPropertyGroup>([&] { return _keyLightProperties; }); }

    void setBackgroundMode(BackgroundMode value) { _backgroundMode = value; _backgroundPropertiesChanged = true; }
    BackgroundMode getBackgroundMode() const { return _backgroundMode; }

    void setHazeMode(const uint32_t value);
    uint32_t getHazeMode() const;

    SkyboxPropertyGroup getSkyboxProperties() const { return resultWithReadLock<SkyboxPropertyGroup>([&] { return _skyboxProperties; }); }
    
    const HazePropertyGroup& getHazeProperties() const { return _hazeProperties; }

    const StagePropertyGroup& getStageProperties() const { return _stageProperties; }

    bool getFlyingAllowed() const { return _flyingAllowed; }
    void setFlyingAllowed(bool value) { _flyingAllowed = value; }
    bool getGhostingAllowed() const { return _ghostingAllowed; }
    void setGhostingAllowed(bool value) { _ghostingAllowed = value; }
    QString getFilterURL() const;
    void setFilterURL(const QString url); 

    bool keyLightPropertiesChanged() const { return _keyLightPropertiesChanged; }
    bool backgroundPropertiesChanged() const { return _backgroundPropertiesChanged; }
    bool skyboxPropertiesChanged() const { return _skyboxPropertiesChanged; }

    bool hazePropertiesChanged() const { 
        return _hazePropertiesChanged; 
    }

    bool stagePropertiesChanged() const { return _stagePropertiesChanged; }

    void resetRenderingPropertiesChanged();

    virtual bool supportsDetailedRayIntersection() const override { return true; }
    virtual bool findDetailedRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                         bool& keepSearching, OctreeElementPointer& element, float& distance,
                         BoxFace& face, glm::vec3& surfaceNormal,
                         void** intersectedObject, bool precisionPicking) const override;

    virtual void debugDump() const override;

    static const ShapeType DEFAULT_SHAPE_TYPE;
    static const QString DEFAULT_COMPOUND_SHAPE_URL;
    static const bool DEFAULT_FLYING_ALLOWED;
    static const bool DEFAULT_GHOSTING_ALLOWED;
    static const QString DEFAULT_FILTER_URL;

    static const uint32_t DEFAULT_HAZE_MODE{ (uint32_t)COMPONENT_MODE_INHERIT };

protected:
    KeyLightPropertyGroup _keyLightProperties;

    ShapeType _shapeType = DEFAULT_SHAPE_TYPE;
    QString _compoundShapeURL;

    BackgroundMode _backgroundMode = BACKGROUND_MODE_INHERIT;

    uint32_t _hazeMode{ DEFAULT_HAZE_MODE };

    SkyboxPropertyGroup _skyboxProperties;
    HazePropertyGroup _hazeProperties;
    StagePropertyGroup _stageProperties;

    bool _flyingAllowed { DEFAULT_FLYING_ALLOWED };
    bool _ghostingAllowed { DEFAULT_GHOSTING_ALLOWED };
    QString _filterURL { DEFAULT_FILTER_URL };

    // Dirty flags turn true when either keylight properties is changing values.
    bool _keyLightPropertiesChanged { false };
    bool _backgroundPropertiesChanged{ false };
    bool _skyboxPropertiesChanged { false };
    bool _hazePropertiesChanged{ false };
    bool _stagePropertiesChanged { false };

    static bool _drawZoneBoundaries;
    static bool _zonesArePickable;
};

#endif // hifi_ZoneEntityItem_h
