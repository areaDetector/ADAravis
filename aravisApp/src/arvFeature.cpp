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
    // Not sure how to get this with Aravis 
    if (!mIsImplemented) return false;
    return true;
}

bool arvFeature::isWritable() { 
    if (!mIsImplemented) return false;
    return !arv_gc_feature_node_is_locked(ARV_GC_FEATURE_NODE(mNode), NULL);
}

epicsInt64 arvFeature::readInteger() { 
    return arv_device_get_integer_feature_value(mDevice, mFeatureName.c_str(), NULL);
}

epicsInt64 arvFeature::readIntegerMin() {
    gint64 min, max;
    arv_device_get_integer_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max, NULL);    
    return min;
}

epicsInt64 arvFeature::readIntegerMax() {
    gint64 min, max;
    arv_device_get_integer_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max, NULL);    
    return max;
}

epicsInt64 arvFeature::readIncrement() { 
    // Not sure how to get the from Aravis
    return 1;
}

void arvFeature::writeInteger(epicsInt64 value) { 
    arv_device_set_integer_feature_value(mDevice, mFeatureName.c_str(), value, NULL);
}

bool arvFeature::readBoolean() { 
    return arv_device_get_boolean_feature_value(mDevice, mFeatureName.c_str(), NULL);
}

void arvFeature::writeBoolean(bool value) { 
    arv_device_set_boolean_feature_value(mDevice, mFeatureName.c_str(), value, NULL);
}

double arvFeature::readDouble() { 
    return arv_device_get_float_feature_value(mDevice, mFeatureName.c_str(), NULL);
}

void arvFeature::writeDouble(double value) { 
    arv_device_set_float_feature_value(mDevice, mFeatureName.c_str(), value, NULL);
}

double arvFeature::readDoubleMin() {
    double min, max;
    arv_device_get_float_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max, NULL);    
    return min;
}

double arvFeature::readDoubleMax() {
    double min, max;
    arv_device_get_float_feature_bounds(mDevice, mFeatureName.c_str(), &min, &max, NULL);    
    return max;
}

int arvFeature::readEnumIndex() {
    return arv_device_get_integer_feature_value(mDevice, mFeatureName.c_str(), NULL);
}

void arvFeature::writeEnumIndex(int value) { 
    arv_device_set_integer_feature_value(mDevice, mFeatureName.c_str(), value, NULL);
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
    const char *pString; 
    pString = arv_device_get_string_feature_value(mDevice, mFeatureName.c_str(), NULL);
    return pString ? pString : "";
}

void arvFeature::writeString(std::string const & value) { 
    arv_device_set_string_feature_value(mDevice, mFeatureName.c_str(), value.c_str(), NULL);
}

void arvFeature::writeCommand() { 
    arv_device_execute_command(mDevice, mFeatureName.c_str(), NULL);
}

void arvFeature::readEnumChoices(std::vector<std::string>& enumStrings, std::vector<int>& enumValues) {
    guint numEnums;
    ArvGcEnumeration *enumeration = (ARV_GC_ENUMERATION (mNode));
//printf("calling arv_gc_enumeration_get_available_int_values for %s\n", mFeatureName.c_str());
    gint64 *values = arv_gc_enumeration_dup_available_int_values(enumeration, &numEnums, NULL);
//printf("calling arv_gc_enumeration_get_available_string_values for %s\n", mFeatureName.c_str());
    const char **strings = arv_gc_enumeration_dup_available_string_values(enumeration, &numEnums, NULL);
//printf("done calling arv_gc_enumeration_get_available_string_values for %s, numEnums=%u\n", mFeatureName.c_str(), numEnums);
    for (unsigned int i=0; i<numEnums; i++) {
        enumStrings.push_back(strings[i]);
        enumValues.push_back(values[i]);
    }
    g_free(strings);
}


