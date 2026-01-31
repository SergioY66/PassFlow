-- PassFlow Database Schema
-- Database: buslocal
-- User: bus / Password: njkmrjbus

-- Create database if not exists
CREATE DATABASE IF NOT EXISTS buslocal;
USE buslocal;

-- Settings table
CREATE TABLE IF NOT EXISTS settings (
    id INT AUTO_INCREMENT PRIMARY KEY,
    doors INT NOT NULL DEFAULT 2 COMMENT 'Number of doors (and cameras)',
    stopBeginDelay INT NOT NULL DEFAULT 5 COMMENT 'Seconds before DOOR_OPEN to start video',
    stopEndDelay INT NOT NULL DEFAULT 5 COMMENT 'Seconds after DOOR_CLOSE to stop video',
    daysBeforeDeliteVideo INT NOT NULL DEFAULT 30 COMMENT 'Days to keep video files',
    cam0_string VARCHAR(512) DEFAULT '' COMMENT 'Connection string for Camera 0',
    cam1_string VARCHAR(512) DEFAULT '' COMMENT 'Connection string for Camera 1',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- Insert default settings if table is empty
INSERT INTO settings (doors, stopBeginDelay, stopEndDelay, daysBeforeDeliteVideo, cam0_string, cam1_string)
SELECT 2, 5, 5, 30, 'udp://@:8080', 'rtsp://admin:password@192.168.1.101:554/stream1'
WHERE NOT EXISTS (SELECT 1 FROM settings LIMIT 1);

-- Remote DB addresses table
CREATE TABLE IF NOT EXISTS remoteDB (
    id INT AUTO_INCREMENT PRIMARY KEY,
    remoteDBAddress VARCHAR(255) NOT NULL COMMENT 'Remote database address for replication',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Events table for logging door/cover/power state changes
CREATE TABLE IF NOT EXISTS events (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    event_type INT NOT NULL COMMENT 'Event type code',
    event_description VARCHAR(255) NOT NULL COMMENT 'Human readable event description',
    event_time VARCHAR(30) NOT NULL COMMENT 'Timestamp of event',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_event_time (event_time),
    INDEX idx_event_type (event_type)
);

-- Video segments table for tracking recorded video files
CREATE TABLE IF NOT EXISTS video_segments (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    camera_id INT NOT NULL COMMENT 'Camera ID (0 or 1)',
    start_time VARCHAR(30) NOT NULL COMMENT 'Video segment start time',
    stop_time VARCHAR(30) NOT NULL COMMENT 'Video segment stop time',
    filename VARCHAR(512) NOT NULL COMMENT 'Path to video file',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_camera_id (camera_id),
    INDEX idx_start_time (start_time)
);

-- Create user if not exists and grant permissions
-- Note: Run these commands as root/admin user
-- CREATE USER IF NOT EXISTS 'bus'@'localhost' IDENTIFIED BY 'njkmrjbus';
-- GRANT ALL PRIVILEGES ON buslocal.* TO 'bus'@'localhost';
-- FLUSH PRIVILEGES;
