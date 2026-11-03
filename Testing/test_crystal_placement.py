#!/usr/bin/env python3
"""
Unit verification for crystal placement calculations.
Checks default central ring placement, custom location override,
and perimeter placement math.
"""
import math

class Vector3:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x, self.y, self.z = float(x), float(y), float(z)

    def __sub__(self, other):
        return Vector3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __add__(self, other):
        return Vector3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __mul__(self, scalar):
        return Vector3(self.x * scalar, self.y * scalar, self.z * scalar)

    def safe_normal_2d(self):
        len_sq = self.x * self.x + self.y * self.y
        if len_sq < 1e-8:
            return Vector3(0, 0, 0)
        l = math.sqrt(len_sq)
        return Vector3(self.x / l, self.y / l, 0.0)

    def to_tuple(self):
        return (round(self.x, 2), round(self.y, 2), round(self.z, 2))


def get_station_crystal_world_location(station_loc, center_loc, ring_radius,
                                       custom_loc=None, use_custom=False):
    if use_custom and custom_loc is not None:
        return center_loc + custom_loc

    radial_dir = (station_loc - center_loc).safe_normal_2d()
    pos = center_loc + (radial_dir * ring_radius)
    pos.z = center_loc.z
    return pos


def test_central_ring_placement():
    center = Vector3(0, 0, 0)
    station = Vector3(332.6, 0, 0)
    ring_radius = 85.0

    loc = get_station_crystal_world_location(station, center, ring_radius)
    assert loc.to_tuple() == (85.0, 0.0, 0.0), f"Expected (85.0, 0.0, 0.0), got {loc.to_tuple()}"


def test_custom_crystal_placement():
    center = Vector3(100, 50, 0)
    station = Vector3(432.6, 50, 0)
    ring_radius = 85.0
    custom_offset = Vector3(200.0, -150.0, 30.0)

    loc = get_station_crystal_world_location(
        station, center, ring_radius, custom_loc=custom_offset, use_custom=True
    )
    assert loc.to_tuple() == (300.0, -100.0, 30.0), f"Expected (300.0, -100.0, 30.0), got {loc.to_tuple()}"


def test_station_perimeter_placement():
    center = Vector3(0, 0, 0)
    station = Vector3(332.6, 0, 0)
    inward_dist = 60.0

    inward_dir = (center - station).safe_normal_2d()
    perimeter_loc = station + (inward_dir * inward_dist)
    assert perimeter_loc.to_tuple() == (272.6, 0.0, 0.0), f"Expected (272.6, 0.0, 0.0), got {perimeter_loc.to_tuple()}"


if __name__ == "__main__":
    test_central_ring_placement()
    test_custom_crystal_placement()
    test_station_perimeter_placement()
    print("All crystal placement tests passed successfully!")
