//
//  AnimSkeleton.cpp
//
//  Created by Anthony J. Thibault on 9/2/15.
//  Copyright (c) 2015 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "AnimSkeleton.h"

#include <glm/gtx/transform.hpp>

#include <GLMHelpers.h>

#include "AnimationLogging.h"

AnimSkeleton::AnimSkeleton(const FBXGeometry& fbxGeometry) {
    // convert to std::vector of joints
    std::vector<FBXJoint> joints;
    joints.reserve(fbxGeometry.joints.size());
    for (auto& joint : fbxGeometry.joints) {
        joints.push_back(joint);
    }

    AnimPose geometryOffset(fbxGeometry.offset);
    buildSkeletonFromJoints(joints, geometryOffset);
}

AnimSkeleton::AnimSkeleton(const std::vector<FBXJoint>& joints, const AnimPose& geometryOffset) {
    buildSkeletonFromJoints(joints, geometryOffset);
}

int AnimSkeleton::nameToJointIndex(const QString& jointName) const {
    for (size_t i = 0; i < _joints.size(); i++) {
        if (_joints[i].name == jointName) {
            return i;
        }
    }
    return -1;
}

int AnimSkeleton::getNumJoints() const {
    return _joints.size();
}

const AnimPose& AnimSkeleton::getAbsoluteBindPose(int jointIndex) const {
    return _absoluteBindPoses[jointIndex];
}

const AnimPose& AnimSkeleton::getRelativeBindPose(int jointIndex) const {
    return _relativeBindPoses[jointIndex];
}

int AnimSkeleton::getParentIndex(int jointIndex) const {
    return _joints[jointIndex].parentIndex;
}

const QString& AnimSkeleton::getJointName(int jointIndex) const {
    return _joints[jointIndex].name;
}

AnimPose AnimSkeleton::getAbsolutePose(int jointIndex, const AnimPoseVec& poses) const {
    if (jointIndex < 0 || jointIndex >= (int)poses.size() || jointIndex >= (int)_joints.size()) {
        return AnimPose::identity;
    } else {
        return getAbsolutePose(_joints[jointIndex].parentIndex, poses) * poses[jointIndex];
    }
}

void AnimSkeleton::buildSkeletonFromJoints(const std::vector<FBXJoint>& joints, const AnimPose& geometryOffset) {
    _joints = joints;

    // build a cache of bind poses
    _absoluteBindPoses.reserve(joints.size());
    _relativeBindPoses.reserve(joints.size());

    // iterate over FBXJoints and extract the bind pose information.
    for (size_t i = 0; i < joints.size(); i++) {
        if (_joints[i].bindTransformFoundInCluster) {
            // Use the FBXJoint::bindTransform, which is absolute model coordinates
            // i.e. not relative to it's parent.
            AnimPose absoluteBindPose(_joints[i].bindTransform);
            _absoluteBindPoses.push_back(absoluteBindPose);
            int parentIndex = getParentIndex(i);
            if (parentIndex >= 0) {
                AnimPose inverseParentAbsoluteBindPose = _absoluteBindPoses[parentIndex].inverse();
                _relativeBindPoses.push_back(inverseParentAbsoluteBindPose * absoluteBindPose);
            } else {
                _relativeBindPoses.push_back(absoluteBindPose);
            }
        } else {
            // use FBXJoint's local transform, instead
            glm::mat4 rotTransform = glm::mat4_cast(_joints[i].preRotation * _joints[i].rotation * _joints[i].postRotation);
            glm::mat4 relBindMat = glm::translate(_joints[i].translation) * _joints[i].preTransform * rotTransform * _joints[i].postTransform;
            AnimPose relBindPose(relBindMat);
            _relativeBindPoses.push_back(relBindPose);

            int parentIndex = getParentIndex(i);
            if (parentIndex >= 0) {
                _absoluteBindPoses.push_back(_absoluteBindPoses[parentIndex] * relBindPose);
            } else {
                _absoluteBindPoses.push_back(relBindPose);
            }
        }
    }

    // now we want to normalize scale from geometryOffset to all poses.
    // This will ensure our bone translations will be in meters, even if the model was authored with some other unit of mesure.
    for (auto& absPose : _absoluteBindPoses) {
        absPose.trans = (geometryOffset * absPose).trans;
        absPose.scale = vec3(1, 1, 1);
    }

    // re-compute relative poses based on the modified absolute poses.
    for (size_t i = 0; i < _relativeBindPoses.size(); i++) {
        int parentIndex = getParentIndex(i);
        if (parentIndex >= 0) {
            _relativeBindPoses[i] = _absoluteBindPoses[parentIndex].inverse() * _absoluteBindPoses[i];
        } else {
            _relativeBindPoses[i] = _absoluteBindPoses[i];
        }
    }
}

#ifndef NDEBUG
void AnimSkeleton::dump() const {
    qCDebug(animation) << "[";
    for (int i = 0; i < getNumJoints(); i++) {
        qCDebug(animation) << "    {";
        qCDebug(animation) << "        index =" << i;
        qCDebug(animation) << "        name =" << getJointName(i);
        qCDebug(animation) << "        absBindPose =" << getAbsoluteBindPose(i);
        qCDebug(animation) << "        relBindPose =" << getRelativeBindPose(i);
        if (getParentIndex(i) >= 0) {
            qCDebug(animation) << "        parent =" << getJointName(getParentIndex(i));
        }
        qCDebug(animation) << "    },";
    }
    qCDebug(animation) << "]";
}

void AnimSkeleton::dump(const AnimPoseVec& poses) const {
    qCDebug(animation) << "[";
    for (int i = 0; i < getNumJoints(); i++) {
        qCDebug(animation) << "    {";
        qCDebug(animation) << "        index =" << i;
        qCDebug(animation) << "        name =" << getJointName(i);
        qCDebug(animation) << "        absBindPose =" << getAbsoluteBindPose(i);
        qCDebug(animation) << "        relBindPose =" << getRelativeBindPose(i);
        qCDebug(animation) << "        pose =" << poses[i];
        if (getParentIndex(i) >= 0) {
            qCDebug(animation) << "        parent =" << getJointName(getParentIndex(i));
        }
        qCDebug(animation) << "    },";
    }
    qCDebug(animation) << "]";
}
#endif
