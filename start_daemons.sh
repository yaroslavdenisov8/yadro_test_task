#!/bin/bash

systemctl --user daemon-reload
systemctl --user start accel_server
systemctl --user start accel_node_b
systemctl --user start accel_node_a
