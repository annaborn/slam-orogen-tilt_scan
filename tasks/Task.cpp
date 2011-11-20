/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "Task.hpp"
#include <envire/maps/LaserScan.hpp>
#include <envire/maps/TriMesh.hpp>
#include <envire/operators/ScanMeshing.hpp>

using namespace tilt_scan;
using namespace envire;

Task::Task(std::string const& name)
    : TaskBase(name)
{
}

Task::Task(std::string const& name, RTT::ExecutionEngine* engine)
    : TaskBase(name, engine)
{
}

Task::~Task()
{
}

void Task::scan_samplesTransformerCallback(const base::Time &ts, const ::base::samples::LaserScan &scan_samples_sample)
{
    // start building up scans until the odometry changes more than a delta
    // with respect to the first recorded pose of the scan, then start a new.
    // only consider scans of a specific number of lines valid

    Eigen::Affine3d body2odometry, laser2body;
    if( !_body2odometry.get( ts, body2odometry ) || !_laser2body.get( ts, laser2body ) )
	return;

    // for now set to 1cm and 1degree 
    // TODO parametrize
    base::PoseUpdateThreshold pose_change_threshold( 0.01, 1.0 / 180.0 * M_PI ); 

    if( scanFrame && !pose_change_threshold.test( scan_body2odometry, body2odometry ) )
    {
	// store laserline for pose 
	
	// get the laserscan and set up the operator chain
	FrameNode* laserFrame = new FrameNode( laser2body );
	env->addChild( scanFrame.get(), laserFrame );

	LaserScan *scanNode = new LaserScan();
	scanNode->addScanLine( 0, scan_samples_sample );
	env->setFrameNode( scanNode, laserFrame );
	
	TriMesh *laserPc = new TriMesh();
	env->setFrameNode( laserPc, laserFrame );

	ScanMeshing *smOp = new ScanMeshing();
	env->attachItem( smOp );
	smOp->addInput( scanNode );
	smOp->addOutput( laserPc );
	smOp->updateAll();

	// add to merge operator
	mergeOp->addOutput( laserPc );
    }
    else
    {
	if( scanFrame )
	{
	    // if the number of scans in the frame is enough, keep and store it
	    size_t numScans = env->getOutputs( mergeOp.get() ).size();

	    size_t numScansThreshold = 100;
	    if( numScans > numScansThreshold )
	    {
		mergeOp->updateAll();
		// TODO reproject pointcloud into distance frame and write to
		// output port if connected
	    }
	    else
	    {
		// TODO remove whole tree of scanframe 
		env->detachItem( scanFrame.get() );
	    }
	}

	// set-up new structure
	// Framenode for the body to odometry
	scanFrame = new envire::FrameNode( body2odometry );
	env->getRootNode()->addChild( scanFrame.get() );

	// and a pointlcoud with the final scan
	envire::Pointcloud *pc = new envire::Pointcloud();
	env->setFrameNode( pc, scanFrame.get() );

	// as well as a merge operator has the pointcloud as output
	mergeOp = new MergePointcloud();
	env->attachItem( mergeOp.get() );
	mergeOp->addOutput( pc );
    }
}

/// The following lines are template definitions for the various state machine
// hooks defined by Orocos::RTT. See Task.hpp for more detailed
// documentation about them.

bool Task::configureHook()
{
    if (! TaskBase::configureHook())
        return false;
    return true;

    env = new envire::Environment();
}
// bool Task::startHook()
// {
//     if (! TaskBase::startHook())
//         return false;
//     return true;
// 
//     env = new envire::Environment();
// }
// void Task::updateHook()
// {
//     TaskBase::updateHook();
// }
// void Task::errorHook()
// {
//     TaskBase::errorHook();
// }
// void Task::stopHook()
// {
//     TaskBase::stopHook();
// }

void Task::cleanupHook()
{
    TaskBase::cleanupHook();

    // write environment, if path is given
    if( !_environment_debug_path.value().empty() )
    {
	envire::Serialization so;
	so.serialize(env, _environment_debug_path.value() );
    }

    delete env;
}

