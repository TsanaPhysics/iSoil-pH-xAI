<?php
/**
 * Project: RBRU Digital Agriphysics & AI Soil pH Monitor
 * File: api/export_csv.php
 * Description: Export database records to downloadable CSV (supports session filtering)
 */

require_once __DIR__ . '/db.php';

$sessionId = isset($_GET['session_id']) ? trim($_GET['session_id']) : '';

try {
    if (!empty($sessionId) && $sessionId !== 'all') {
        $stmt = $db->prepare("SELECT id, session_id, timestamp, voltage, temp_c, ph_traditional, ph_ai, delta_error, target_buffer, soil_status, sample_note 
            FROM measurements 
            WHERE session_id = :session_id 
            ORDER BY id ASC");
        $stmt->execute([':session_id' => $sessionId]);
        $filename = "soil_ph_" . strtolower($sessionId) . "_" . date('Ymd_His') . ".csv";
    } else {
        $stmt = $db->query("SELECT id, session_id, timestamp, voltage, temp_c, ph_traditional, ph_ai, delta_error, target_buffer, soil_status, sample_note 
            FROM measurements 
            ORDER BY id ASC");
        $filename = "soil_ph_all_sessions_" . date('Ymd_His') . ".csv";
    }
    
    header('Content-Type: text/csv; charset=utf-8');
    header('Content-Disposition: attachment; filename=' . $filename);
    
    $output = fopen('php://output', 'w');
    
    // เขียน BOM สำหรับ UTF-8 ให้เปิดใน Excel ได้ถูกต้อง
    fprintf($output, chr(0xEF).chr(0xBB).chr(0xBF));
    
    // เขียนหัวคอลัมน์
    fputcsv($output, [
        'Record_ID',
        'Session_ID',
        'DateTime',
        'Date',
        'Time',
        'Cell_Potential_V',
        'Temperature_C',
        'pH_Traditional_Nernst',
        'pH_AI_Predicted',
        'Delta_Compensation_pH',
        'Target_Buffer',
        'Agronomy_Status',
        'Sample_Note'
    ]);
    
    while ($row = $stmt->fetch()) {
        $parts = explode(' ', $row['timestamp']);
        $dateOnly = $parts[0] ?? '';
        $timeOnly = $parts[1] ?? '';
        fputcsv($output, [
            $row['id'],
            $row['session_id'] ?? 'EXP_001',
            $row['timestamp'],
            $dateOnly,
            $timeOnly,
            $row['voltage'],
            $row['temp_c'],
            $row['ph_traditional'],
            $row['ph_ai'],
            $row['delta_error'],
            $row['target_buffer'],
            $row['soil_status'],
            $row['sample_note']
        ]);
    }
    
    fclose($output);
    exit;
} catch (PDOException $e) {
    header('Content-Type: text/plain; charset=utf-8');
    echo "Error exporting CSV: " . $e->getMessage();
}
