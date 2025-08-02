#!/usr/bin/env python3
"""
测试快速跳出功能的简单脚本
"""

# 模拟配置值
VOLTAGE_TOLERANCE_PERCENT = 3.0
QUICK_EXIT_THRESHOLD = 5.0

def test_error_classification():
    """测试误差分类逻辑"""
    
    test_cases = [
        (1.5, 'excellent'),  # 1.5% - 优秀
        (2.8, 'compliant'),  # 2.8% - 合规
        (4.2, 'quick_exit'), # 4.2% - 快速跳出
        (6.8, 'failed'),     # 6.8% - 失败
    ]
    
    print("=== 误差分类测试 ===")
    print(f"合规阈值: {VOLTAGE_TOLERANCE_PERCENT}%")
    print(f"快速跳出阈值: {QUICK_EXIT_THRESHOLD}%")
    print()
    
    for error_percent, expected in test_cases:
        print(f"误差: {error_percent}%")
        
        if error_percent <= VOLTAGE_TOLERANCE_PERCENT:
            status = "✓ 校准成功(合规)"
            should_exit = True
        elif error_percent <= QUICK_EXIT_THRESHOLD:
            status = "✓ 校准成功(快速跳出)"
            should_exit = True
        else:
            status = "⚠ 校准误差超标"
            should_exit = False
            
        print(f"  状态: {status}")
        print(f"  是否跳出: {should_exit}")
        print(f"  期望分类: {expected}")
        print()

if __name__ == "__main__":
    test_error_classification()
