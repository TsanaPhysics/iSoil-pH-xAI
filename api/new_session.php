<?php
/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: api/new_session.php
 * Description: API endpoint to initialize or request a new experiment session
 */

header('Content-Type: application/json; charset=utf-8');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

require_once __DIR__ . '/db.php';

try {
    // ดึงเซสชันล่าสุดจากฐานข้อมูลเพื่อคำนวณหมายเลขเซสชันถัดไป
    $stmt = $db->query("SELECT session_id FROM measurements WHERE session_id LIKE 'EXP_%' ORDER BY id DESC LIMIT 1");
    $lastRow = $stmt->fetch();
    
    $nextNum = 1;
    if ($lastRow && preg_match('/EXP_(\d+)/', $lastRow['session_id'], $m)) {
        $nextNum = intval($m[1]) + 1;
    }
    
    $newSessionId = sprintf("EXP_%03d", $nextNum);

    // เขียนคำสั่งลง command file สำหรับ serial bridge
    $cmdData = [
        'command' => 'START_NEW',
        'session_id' => $newSessionId,
        'timestamp' => date('Y-m-d H:i:s')
    ];
    $cmdFile = __DIR__ . '/../data/bridge_cmd.json';
    file_put_contents($cmdFile, json_encode($cmdData, JSON_PRETTY_PRINT));

    echo json_encode([
        'success' => true,
        'session_id' => $newSessionId,
        'message' => 'New session requested successfully',
        'timestamp' => date('Y-m-d H:i:s')
    ], JSON_UNESCAPED_UNICODE);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode([
        'success' => false,
        'error' => $e->getMessage()
    ], JSON_UNESCAPED_UNICODE);
}
