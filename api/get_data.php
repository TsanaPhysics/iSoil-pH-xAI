<?php
/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: api/get_data.php
 * Description: REST API for querying real-time telemetry, historical trend and statistics
 */

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');

require_once __DIR__ . '/db.php';

$limit = isset($_GET['limit']) ? intval($_GET['limit']) : 50;
if ($limit < 5) $limit = 5;
if ($limit > 500) $limit = 500;

try {
    // 1. ดึงข้อมูลล่าสุด 1 รายการ
    $latestStmt = $db->query("SELECT * FROM measurements ORDER BY id DESC LIMIT 1");
    $latest = $latestStmt->fetch();

    // 2. ดึงข้อมูลย้อนหลังตามจำนวนที่กำหนด (เรียงลำดับเวลา ASC เพื่อพล็อตกราฟ)
    $historyStmt = $db->prepare("SELECT * FROM (
        SELECT * FROM measurements ORDER BY id DESC LIMIT :limit
    ) ORDER BY id ASC");
    $historyStmt->bindValue(':limit', $limit, PDO::PARAM_INT);
    $historyStmt->execute();
    $history = $historyStmt->fetchAll();

    // 3. คำนวณสถิติภาพรวม (KPI Metrics)
    $statsStmt = $db->query("SELECT 
        COUNT(*) as total_count,
        AVG(ph_ai) as avg_ph_ai,
        MIN(ph_ai) as min_ph_ai,
        MAX(ph_ai) as max_ph_ai,
        AVG(voltage) as avg_voltage,
        AVG(delta_error) as avg_error,
        SUM(CASE WHEN ph_ai >= 5.5 AND ph_ai <= 6.5 THEN 1 ELSE 0 END) as optimal_durian_count,
        SUM(CASE WHEN ph_ai < 5.0 THEN 1 ELSE 0 END) as acidic_count
    FROM measurements");
    $stats = $statsStmt->fetch();

    echo json_encode([
        'success' => true,
        'timestamp' => date('Y-m-d H:i:s'),
        'latest' => $latest ?: null,
        'history' => $history,
        'stats' => [
            'total_count' => intval($stats['total_count']),
            'avg_ph_ai' => $stats['avg_ph_ai'] ? round(floatval($stats['avg_ph_ai']), 2) : 7.00,
            'min_ph_ai' => $stats['min_ph_ai'] ? round(floatval($stats['min_ph_ai']), 2) : 7.00,
            'max_ph_ai' => $stats['max_ph_ai'] ? round(floatval($stats['max_ph_ai']), 2) : 7.00,
            'avg_voltage' => $stats['avg_voltage'] ? round(floatval($stats['avg_voltage']), 3) : 1.650,
            'avg_error' => $stats['avg_error'] ? round(floatval($stats['avg_error']), 3) : 0.000,
            'optimal_durian_count' => intval($stats['optimal_durian_count']),
            'acidic_count' => intval($stats['acidic_count']),
            'optimal_percentage' => $stats['total_count'] > 0 
                ? round((intval($stats['optimal_durian_count']) / intval($stats['total_count'])) * 100, 1) 
                : 0
        ]
    ]);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'error' => 'Query failed: ' . $e->getMessage()
    ]);
}
