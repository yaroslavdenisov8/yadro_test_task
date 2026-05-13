#!/bin/bash

systemctl --user daemon-reload
systemctl --user stop accel_node_a
systemctl --user stop accel_node_b
systemctl --user stop accel_server
