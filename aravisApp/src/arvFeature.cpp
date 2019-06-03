// arvFeature.cpp
// Mark Rivers
// October 26, 2018

#include <arvFeature.h>

arvFeature::arvFeature(GenICamFeatureSet *set, 
                       std::string const & asynName, asynParamType asynType, int asynIndex,
                       std::string const & featureName, GCFeatureType_t featureType, ArvDevice *device)
                     
    : GenICamFeature(set, asynName, asynType, asynIndex, featureName, featureType),
    mNode(0), mDevice(device)         
{
    mNode = arv_device_get_feature(mDevice, featureName.c_str());
    if (mNode) {
        mIsImplemented = arv_gc_feature_node_is_implemented(ARV_GC_FEATURE_NODE(mNode), NULL);
    } else {
        mIsImplemented = false;
    }
}

bool arvFeature::isImplemented() { 
    return mIsImplemented; 
}

bool arvFeature::isAvailable() { 
    return arv_gc_feature_node_is_available(ARV_GC_FEATURE_NODE(mNode), NULL);
}

bool arvFeature::isReadable() {
    // Note sure how to get this with Aravis 
    return true;
}

bool arvFeature::isWritable() { 
    return !arv_gc_feature_node_is_locked(ARV_GC_FEATURE_NODE(mNode), NULL);
}

int arvFeature::readInteger() { 
    return arv_device_get_integer_feature_value(mDevice, mFeatureName.c_str());
}

int arvFeature::readIntegerMin() {
    gint64 min, max;
    arv_device_get_integer_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max);    
    return min;
}

int arvFeature::readIntegerMax() {
    gint64 min, max;
    arv_device_get_integer_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max);    
    return max;
}

int arvFeature::readIncrement() { 
    // Not sure how to get the from Aravis
    return 1;
}

void arvFeature::writeInteger(int value) { 
    arv_device_set_integer_feature_value(mDevice, mFeatureName.c_str(), value);
}

bool arvFeature::readBoolean() { 
    return arv_device_get_integer_feature_value(mDevice, mFeatureName.c_str());
}

void arvFeature::writeBoolean(bool value) { 
    arv_device_set_integer_feature_value(mDevice, mFeatureName.c_str(), value ? 1 : 0);
}

double arvFeature::readDouble() { 
    return arv_device_get_float_feature_value(mDevice, mFeatureName.c_str());
}

void arvFeature::writeDouble(double value) { 
    arv_device_set_float_feature_value(mDevice, mFeatureName.c_str(), value);
}

double arvFeature::readDoubleMin() {
    double min, max;
    arv_device_get_float_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max);    
    return min;
}

double arvFeature::readDoubleMax() {
    double min, max;
    arv_device_get_float_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max);    
    return max;
}

int arvFeature::readEnumIndex() {
    return arv_device_get_integer_feature_value(mDevice, mFeatureName.c_str());
}

void arvFeature::writeEnumIndex(int value) { 
    arv_device_set_integer_feature_value(mDevice, mFeatureName.c_str(), value);
}

std::string arvFeature::readEnumString() { 
    const char *pString;
    pString = arv_gc_feature_node_get_value_as_string(ARV_GC_FEATURE_NODE(mNode), NULL);
    if (pString == 0) pString = "";
    return std::string(pString);
}

void arvFeature::writeEnumString(std::string const &value) { 
}

std::string arvFeature::readString() { 
    return arv_device_get_string_feature_value(mDevice, mFeatureName.c_str());
}

void arvFeature::writeString(std::string const & value) { 
    arv_device_set_string_feature_value(mDevice, mFeatureName.c_str(), value.c_str());
}

void arvFeature::writeCommand() { 
    arv_device_execute_command(mDevice, mFeatureName.c_str());
}

void arvFeature::readEnumChoices(std::vector<std::string>& enumStrings, std::vector<int>& enumValues) {
    guint numEnums;
    ArvGcEnumeration *enumeration = (ARV_GC_ENUMERATION (mNode));
    gint64 *values = arv_gc_enumeration_get_available_int_values(enumeration, &numEnums, NULL);
    const char **strings = arv_gc_enumeration_get_available_string_values(enumeration, &numEnums, NULL);
    for (unsigned int i=0; i<numEnums; i++) {
        enumStrings.push_back(strings[i]);
        enumValues.push_back(values[i]);
    }
    g_free(strings);
}


