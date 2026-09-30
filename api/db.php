<?php
/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: api/db.php
 * Description: SQLite3 Database Connection & Auto-Migration
 */

date_default_timezone_set('Asia/Bangkok');

$dbDir = __DIR__ . '/../data';
if (!is_dir($dbDir)) {
    mkdir($dbDir, 0777, true);
}

$dbPath = $dbDir . '/soil_ph.db';

try {
    $db = new PDO("sqlite:" . $dbPath);
    $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    $db->setAttribute(PDO::ATTR_DEFAULT_FETCH_MODE, PDO::FETCH_ASSOC);

    // สร้างตารางข้อมูลหากยังไม่มี
    $db->exec("CREATE TABLE IF NOT EXISTS measurements (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT DEFAULT 'EXP_001',
        timestamp DATETIME DEFAULT (datetime('now', 'localtime')),
        voltage REAL NOT NULL,
        temp_c REAL NOT NULL,
        ph_traditional REAL NOT NULL,
        ph_ai REAL NOT NULL,
        delta_error REAL NOT NULL,
        target_buffer TEXT DEFAULT 'FIELD',
        soil_status TEXT DEFAULT 'NORMAL',
        sample_note TEXT DEFAULT ''
    )");

    // ตรวจสอบและเพิ่มคอลัมน์ session_id สำหรับฐานข้อมูลเดิม
    $cols = $db->query("PRAGMA table_info(measurements)")->fetchAll();
    $hasSession = false;
    foreach ($cols as $c) {
        if ($c['name'] === 'session_id') {
            $hasSession = true;
            break;
        }
    }
    if (!$hasSession) {
        $db->exec("ALTER TABLE measurements ADD COLUMN session_id TEXT DEFAULT 'EXP_001'");
    }

    $db->exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON measurements(timestamp)");
    $db->exec("CREATE INDEX IF NOT EXISTS idx_session ON measurements(session_id)");
} catch (PDOException $e) {
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode(['success' => false, 'error' => 'Database error: ' . $e->getMessage()]);
    exit;
}
