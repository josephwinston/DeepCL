// Copyright Hugh Perkins 2015 hughperkins at gmail
//
// This Source Code Form is subject to the terms of the Mozilla Public License, 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.

#include "NormalizationHelper.h"
#include "NeuralNet.h"
#include "LossLayer.h"
#include "SoftMaxLayer.h"
#include "InputLayer.h"
#include "LayerMaker.h"
#include "InputLayerMaker.h"

#include "MultiNet.h"

using namespace std;

#undef STATIC
#undef VIRTUAL
#define STATIC
#define VIRTUAL

MultiNet::MultiNet( int numNets, NeuralNet *model ) :
        results( 0 ),
        batchSize( 0 ),
        allocatedSize( 0 ),
        proxyInputLayer( 0 ),
        lossLayer( 0 ) {
//    trainables.push_back( model );
    for( int i = 0; i < numNets; i++ ) {
        trainables.push_back( model->clone() );
    }
    InputLayerMaker<float> *inputLayerMaker = InputLayerMaker<float>::instance();
    inputLayerMaker->numPlanes( trainables[0]->getOutputPlanes() );
    inputLayerMaker->imageSize( trainables[0]->getOutputImageSize() );
    proxyInputLayer = new InputLayer<float>( inputLayerMaker );
    lossLayer = dynamic_cast< LossLayer *>( trainables[0]->cloneLossLayerMaker()->createLayer(proxyInputLayer) );
}
VIRTUAL MultiNet::~MultiNet() {
    if( proxyInputLayer != 0 ) {
        delete proxyInputLayer;
    }
    if( lossLayer != 0 ) {
        delete lossLayer;
    }
    if( results != 0 ) {
        delete[] results;
    }
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        delete (*it);
    }    
}
VIRTUAL int MultiNet::getInputCubeSize() const {
    return trainables[0]->getInputCubeSize();
}
VIRTUAL int MultiNet::getOutputCubeSize() const {
    return trainables[0]->getOutputCubeSize();
}
VIRTUAL int MultiNet::getResultsSize() const {
    return trainables[0]->getResultsSize();
}
VIRTUAL int MultiNet::getOutputPlanes() const {
    return trainables[0]->getOutputPlanes();
}
VIRTUAL int MultiNet::getOutputImageSize() const {
    return trainables[0]->getOutputImageSize();
}
VIRTUAL LossLayerMaker *MultiNet::cloneLossLayerMaker() const {
    throw runtime_error("need to implement MultiNet::cloneLossLayerMaker :-)" );
//    return dynamic_cast< LossLayerMaker *>( lossLayer->maker->clone( clonePreviousLayer ) );
}
VIRTUAL float MultiNet::calcLoss(float const *expectedValues ) {
    float loss = lossLayer->calcLoss( expectedValues );
    return loss;

    // average across all, and then calc loss, right?
    // but .... we need a loss layer?
    // maybe just report average/total child loss, for now?
//    float totalLoss = 0.0f;
//    const int resultsSize = trainables[0]->getResultsSize();
//    float *expectedValuesSum = new 
//    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
//        //totalLoss += (*it)->calcLoss( expectedValues );
//        
//    }
//    return totalLoss;
}
VIRTUAL float MultiNet::calcLossFromLabels(int const *labels ) {
    // average across all, and then calc loss, right?
    // but .... we need a loss layer?
    // maybe just report average/total child loss, for now?
//    float totalLoss = 0.0f;
//    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
//        totalLoss += (*it)->calcLossFromLabels( labels );
//    }
//    return totalLoss;
    SoftMaxLayer *softMaxLayer = dynamic_cast< SoftMaxLayer *>( lossLayer );
    if( softMaxLayer == 0 ) {
        throw runtime_error("trying to call multinet::calcNumRight, but model networks dont have a SoftMax loss layer");
    }
    return softMaxLayer->calcLossFromLabels( labels );
}
VIRTUAL void MultiNet::setBatchSize( int batchSize ) {
    // do children first
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        (*it)->setBatchSize( batchSize );
    }
    proxyInputLayer->setBatchSize( batchSize );
    lossLayer->setBatchSize( batchSize );
    // now ourselves :-)
    if( batchSize <= allocatedSize ) {
        this->batchSize = batchSize;
        return;
    }
    if( results != 0 ) {
        delete[] results;
    }
    this->batchSize = batchSize;
    this->allocatedSize = batchSize;
    results = new float[ trainables[0]->getResultsSize() ];
}
VIRTUAL void MultiNet::setTraining( bool training ) {
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        (*it)->setTraining( training );
    }
}
VIRTUAL int MultiNet::calcNumRight( int const *labels ) {
//    cout << proxyInputLayer->asString() << endl;
//    cout << lossLayer->asString() << endl;
//    proxyInputLayer->in( trainables[0]->getResults() );
//    return dynamic_cast< SoftMaxLayer *>( lossLayer )->calcNumRight( labels );
//    return trainables[0]->calcNumRight( labels );

    // call getResults(), then work out the predictions, then compare with the labels
    // or, use a losslayer?
    // depends on the configuration of the softmax layer too, ie per-plane or not
//    SoftMaxMaker *maker = trainables[0]->cloneLossLayerMaker();
//    SoftMaxLayer *clonedSoftMax = 
    SoftMaxLayer *softMaxLayer = dynamic_cast< SoftMaxLayer *>( lossLayer );
    if( softMaxLayer == 0 ) {
        throw runtime_error("trying to call multinet::calcNumRight, but model networks dont have a SoftMax loss layer");
    }
    return softMaxLayer->calcNumRight( labels );
}
void MultiNet::propagateToOurselves() {
    // now propagate to ourselves :-)
    // I suppose this could be done in GPU, but what if we want to split across mpi?
    const int resultsSize = trainables[0]->getResultsSize();
    memset( results, 0, sizeof( float ) * resultsSize );
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        float const*childResults = (*it)->getResults();
        for( int i = 0; i < resultsSize; i++ ) {
            results[i] += childResults[i];
        }
    }    
    const int numChildren = (int)trainables.size();
    for( int i = 0; i < resultsSize; i++ ) {
        results[i] /= numChildren;
    }
    memcpy( dynamic_cast< SoftMaxLayer * >( lossLayer )->results, results, sizeof(float) * lossLayer->getResultsSize() );
//    proxyInputLayer->in( results );
}
VIRTUAL void MultiNet::propagate( float const*images) {
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        (*it)->propagate( images );
    }
    propagateToOurselves();
}
VIRTUAL void MultiNet::propagate( unsigned char const*images) {
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        (*it)->propagate( images );
    }
    propagateToOurselves();
}
VIRTUAL void MultiNet::backPropFromLabels( float learningRate, int const *labels) {
    // dont think we need to backprop onto ourselves?  Just direclty onto children, right?
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        (*it)->backPropFromLabels( learningRate, labels );
    }
}
VIRTUAL void MultiNet::backProp( float learningRate, float const *expectedResults) {
    for( vector< Trainable * >::iterator it = trainables.begin(); it != trainables.end(); it++ ) {
        (*it)->backProp( learningRate, expectedResults );
    }
}
VIRTUAL float const *MultiNet::getResults() const {
    return results;
}

