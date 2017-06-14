#ifndef SLAM_CTOR_CORE_WEIGHTED_MEAN_DISCREPANCY_SP_ESTIMATOR
#define SLAM_CTOR_CORE_WEIGHTED_MEAN_DISCREPANCY_SP_ESTIMATOR

#include <cmath>
#include "grid_scan_matcher.h"
#include "../maps/grid_rasterization.h"

class WeightedMeanDiscrepancySPEstimator : public ScanProbabilityEstimator {
public:

  virtual LaserScan2D filter_scan(const LaserScan2D &raw_scan,
                                  const GridMap &map) {
    LaserScan2D scan;
    scan.trig_cache = raw_scan.trig_cache;

    auto &scan_pts = scan.points();
    for (const auto &scan_point : raw_scan.points()) {
      auto area_id = map.world_to_cell(scan_point.x(), scan_point.y());
      if (should_skip_point(scan_point, map, area_id)) { continue; }

      scan_pts.push_back(scan_point);
    }

    return scan;
  }

  double estimate_scan_probability(const LaserScan2D &scan,
                                   const RobotPose &pose,
                                   const GridMap &map,
                                   const SPEParams &params) const override {
    auto total_weight = double{0};
    auto total_probability = double{0};

    const auto Observation = expected_scan_point_observation();
    scan.trig_cache->set_theta(pose.theta);
    for (const auto &sp : scan.points()) {
      // FIXME: assumption - sensor pose is in robot's (0,0), dir - 0
      auto world_point = params.scan_is_prerotated ?
        sp.move_origin(pose.x, pose.y) :
        sp.move_origin(pose.x, pose.y, scan.trig_cache);

      auto sp_prob = world_point_probability(world_point, pose, map,
                                             Observation, params);
      auto sp_weight = scan_point_weight(sp);
      total_probability += sp_prob * sp_weight;
      total_weight += sp_weight;
    }
    if (total_weight == 0) {
      // TODO: replace with writing to a proper logger
      std::clog << "WARNING: unknown probability" << std::endl;
      return unknown_probability();
    }
    return total_probability / total_weight;
  }

protected:
  virtual AreaOccupancyObservation expected_scan_point_observation() const {
    return {true, {1.0, 1.0}, {0, 0}, 1.0};
  }

  virtual bool should_skip_point(const ScanPoint2D &sp,
                                 const GridMap &map,
                                 const GridMap::Coord &area_id) const {
    return !sp.is_occupied() || !map.has_cell(area_id);
  }

  virtual double scan_point_weight(const ScanPoint2D &) const {
    return 1;
  }

  double world_point_probability(const Point2D &wp,
                                 const RobotPose &pose, const GridMap &map,
                                 const AreaOccupancyObservation &obs,
                                 const SPEParams &params) const {
    auto analysis_area = params.sp_analysis_area.move_center(wp);
    auto area_ids = GridRasterizedRectangle{map, analysis_area};
    double highest_probability = 0;

    while (area_ids.has_next()) {
      double point_prob = 1.0 - map[area_ids.next()].discrepancy(obs);
      highest_probability = std::max(point_prob, highest_probability);
    }
    return highest_probability;
  }
};

#endif
