import sys
import numpy as np
import yaml
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QTableWidget, QTableWidgetItem,
    QHeaderView, QFileDialog, QGroupBox, QMessageBox
)
from PySide6.QtCore import Qt

class PointCalculator(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AR码-区域坐标计算器")
        self.setGeometry(300, 300, 800, 600)
        
        # 主控件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # AR码区域
        aruco_group = QGroupBox("AR码设置")
        aruco_layout = QVBoxLayout(aruco_group)
        
        # ID输入
        id_layout = QHBoxLayout()
        id_layout.addWidget(QLabel("AR码ID:"))
        self.aruco_id_edit = QLineEdit("582")
        self.aruco_id_edit.setMaximumWidth(100)
        id_layout.addWidget(self.aruco_id_edit)
        id_layout.addStretch()
        aruco_layout.addLayout(id_layout)
        
        # 顶点表格
        aruco_layout.addWidget(QLabel("四个顶点坐标 (mm):"))
        self.aruco_table = QTableWidget(4, 3)
        self.aruco_table.setHorizontalHeaderLabels(["X", "Y", "Z"])
        self.aruco_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        aruco_layout.addWidget(self.aruco_table)
        
        # 区域设置
        regions_group = QGroupBox("功能区域设置 (mm)")
        regions_layout = QVBoxLayout(regions_group)
        
        # 区域表格
        self.regions_table = QTableWidget(0, 4)
        self.regions_table.setHorizontalHeaderLabels(["区域名称", "X", "Y", "Z"])
        self.regions_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        
        # 操作按钮
        btn_layout = QHBoxLayout()
        add_region_btn = QPushButton("+ 添加区域")
        add_region_btn.clicked.connect(self.add_region_row)
        remove_region_btn = QPushButton("- 删除区域")
        remove_region_btn.clicked.connect(self.remove_region_row)
        btn_layout.addWidget(add_region_btn)
        btn_layout.addWidget(remove_region_btn)
        
        regions_layout.addWidget(self.regions_table)
        regions_layout.addLayout(btn_layout)
        
        # 计算结果
        result_group = QGroupBox("计算结果 (m)")
        result_layout = QVBoxLayout(result_group)
        
        # 原点显示
        origin_layout = QHBoxLayout()
        origin_layout.addWidget(QLabel("AR码中心坐标:"))
        self.origin_label = QLabel("未计算")
        origin_layout.addWidget(self.origin_label)
        origin_layout.addStretch()
        result_layout.addLayout(origin_layout)
        
        # 结果表格
        self.result_table = QTableWidget(0, 4)
        self.result_table.setHorizontalHeaderLabels(["区域", "rel_X", "rel_Y", "rel_Z"])
        self.result_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        result_layout.addWidget(self.result_table)
        
        # 主操作按钮
        action_layout = QHBoxLayout()
        self.calc_btn = QPushButton("计算坐标")
        self.calc_btn.clicked.connect(self.calculate_coords)
        self.calc_btn.setStyleSheet("background-color: #4CAF50; color: white;")
        self.export_btn = QPushButton("导出YAML")
        self.export_btn.clicked.connect(self.export_yaml)
        self.export_btn.setStyleSheet("background-color: #2196F3; color: white;")
        self.export_btn.setEnabled(False)
        
        action_layout.addWidget(self.calc_btn)
        action_layout.addWidget(self.export_btn)
        action_layout.addStretch()
        
        # 添加到主布局
        main_layout.addWidget(aruco_group)
        main_layout.addWidget(regions_group)
        main_layout.addWidget(result_group)
        main_layout.addLayout(action_layout)
        
        # 添加示例数据
        self.add_demo_data()
        
        # 状态栏
        self.statusBar().showMessage("准备就绪")
    
    def add_region_row(self):
        row_count = self.regions_table.rowCount()
        self.regions_table.insertRow(row_count)
        
        # 添加默认名称
        name_item = QTableWidgetItem(f"区域{row_count+1}")
        self.regions_table.setItem(row_count, 0, name_item)
        
        # 自动选择第一列
        self.regions_table.editItem(name_item)
        
        self.statusBar().showMessage(f"已添加区域 {row_count+1}")
    
    def remove_region_row(self):
        if self.regions_table.rowCount() > 0:
            row = self.regions_table.currentRow()
            if row == -1:
                row = self.regions_table.rowCount() - 1
            self.regions_table.removeRow(row)
            self.statusBar().showMessage(f"已删除区域")
        else:
            self.statusBar().showMessage("没有可删除的区域")
    
    def add_demo_data(self):
        """添加示例数据便于用户快速上手"""
        # AR码顶点（单位：mm）
        demo_aruco = [
            [100, 150, 0],    # 0.1m, 0.15m, 0m
            [-100, 150, 0],   # -0.1m, 0.15m, 0m
            [-100, -150, 0],  # -0.1m, -0.15m, 0m
            [100, -150, 0]    # 0.1m, -0.15m, 0m
        ]
        
        for row in range(4):
            for col in range(3):
                item = QTableWidgetItem(f"{demo_aruco[row][col]}")
                self.aruco_table.setItem(row, col, item)
        
        # 示例区域（单位：mm）
        demo_regions = {
            "Mute": [-150, 250, 0],  # -0.15m, 0.25m, 0m
            "Reset": [-50, 250, 0],   # -0.05m, 0.25m, 0m
            "1": [50, 250, 0],      # 0.05m, 0.25m, 0m
            "Confirm": [150, 250, 0],   # 0.15m, 0.25m, 0m
            "SelfTest": [50, -250, 0],   # 0.05m, -0.25m, 0m
            "Menu": [-50, -250, 0],  # -0.05m, -0.25m, 0m
            "2": [-150, -250, 0]    # -0.15m, -0.25m, 0m
        }
        
        for name, coords in demo_regions.items():
            row = self.regions_table.rowCount()
            self.regions_table.insertRow(row)
            
            self.regions_table.setItem(row, 0, QTableWidgetItem(name))
            for i, val in enumerate(coords):
                self.regions_table.setItem(row, i+1, QTableWidgetItem(f"{val}"))
    
    def validate_table(self, table, min_rows=1, required_cols=None):
        """验证表格数据完整性"""
        if table.rowCount() < min_rows:
            return f"至少需要{min_rows}行数据", False
        
        if required_cols is not None:
            for row in range(table.rowCount()):
                for col in required_cols:
                    item = table.item(row, col)
                    if not item or not item.text().strip():
                        return f"第{row+1}行第{col+1}列不能为空", False
        
        return "", True
    
    def get_table_data(self, table, str_cols=None, num_cols=None):
        """获取表格数据并验证数字格式"""
        data = []
        
        for row in range(table.rowCount()):
            row_data = []
            for col in range(table.columnCount()):
                item = table.item(row, col)
                if item:
                    value = item.text().strip()
                    # 检查是否数字列
                    if num_cols and col in num_cols:
                        try:
                            value = float(value)
                        except ValueError:
                            return None, f"第{row+1}行第{col+1}列 '{value}' 不是有效数字"
                    # 检查是否字符串列
                    elif str_cols and col in str_cols:
                        if not value:
                            return None, f"第{row+1}行第{col+1}列不能为空"
                    
                    row_data.append(value)
                else:
                    return None, f"第{row+1}行第{col+1}列缺失数据"
            data.append(row_data)
        
        return data, ""
    
    def calculate_coords(self):
        """计算相对坐标"""
        # 验证AR码顶点
        error_msg, is_valid = self.validate_table(self.aruco_table, 4, [0,1,2])
        if not is_valid:
            QMessageBox.warning(self, "AR码设置错误", error_msg)
            return
        
        # 获取AR码ID
        try:
            aruco_id = int(self.aruco_id_edit.text())
        except ValueError:
            QMessageBox.warning(self, "ID格式错误", "AR码ID必须是整数")
            return
        
        # 获取AR码顶点数据（单位：mm）
        vertices, error_msg = self.get_table_data(
            self.aruco_table, 
            num_cols=[0,1,2]
        )
        if not vertices:
            QMessageBox.warning(self, "AR码数据错误", error_msg)
            return
        
        # 转换为米并计算AR码中心点
        vertex_array = np.array([v[:3] for v in vertices]) / 1000  # 毫米转米
        origin = np.mean(vertex_array, axis=0).tolist()
        self.origin_label.setText(f"[{origin[0]:.3f}, {origin[1]:.3f}, {origin[2]:.3f}]")
        
        # 验证区域数据
        error_msg, is_valid = self.validate_table(self.regions_table, 1, [0,1,2])
        if not is_valid:
            QMessageBox.warning(self, "区域设置错误", error_msg)
            return
        
        # 获取区域点数据（单位：mm）
        regions_data, error_msg = self.get_table_data(
            self.regions_table,
            str_cols=[0],
            num_cols=[1,2,3]
        )
        if regions_data is None:
            QMessageBox.warning(self, "区域数据错误", error_msg)
            return
        
        # 处理区域数据（转换为米）
        self.rel_positions = {}
        self.result_table.setRowCount(len(regions_data))
        
        for i, row in enumerate(regions_data):
            name = row[0]
            coords = [x / 1000 for x in row[1:4]]  # 毫米转米
            
            # 计算相对坐标
            rel_coords = [round(c - origin[j], 3) for j, c in enumerate(coords)]
            self.rel_positions[name] = rel_coords
            
            # 添加到结果表格
            self.result_table.setItem(i, 0, QTableWidgetItem(name))
            for j, val in enumerate(rel_coords):
                self.result_table.setItem(i, j+1, QTableWidgetItem(f"{val:.3f}"))
        
        self.export_btn.setEnabled(True)
        self.statusBar().showMessage(f"完成计算: {len(regions_data)}个区域点")
    
    def export_yaml(self):
        """导出YAML配置文件"""
        if not hasattr(self, 'rel_positions') or not self.rel_positions:
            QMessageBox.warning(self, "导出错误", "请先计算坐标")
            return
        
        try:
            aruco_id = int(self.aruco_id_edit.text())
        except ValueError:
            QMessageBox.warning(self, "ID格式错误", "AR码ID必须是整数")
            return
        
        # 构建YAML结构
        yaml_data = {
            "aruco_id": aruco_id,
            "coordinate_system": {
                "origin": [0.0, 0.0, 0.0],
                "orientation": "XY_plane"
            },
            "regions": {
                name: {"rel_position": pos} 
                for name, pos in self.rel_positions.items()
            }
        }
        
        # 选择保存位置
        file_path, _ = QFileDialog.getSaveFileName(
            self, 
            "保存配置", 
            "", 
            "YAML文件 (*.yaml)"
        )
        
        if file_path:
            if not file_path.lower().endswith('.yaml'):
                file_path += '.yaml'
            
            try:
                with open(file_path, 'w', encoding='utf-8') as f:
                    yaml.dump(
                        yaml_data, 
                        f, 
                        default_flow_style=None, 
                        sort_keys=False,
                        allow_unicode=True
                    )
                
                QMessageBox.information(
                    self, 
                    "导出成功", 
                    f"配置已保存至:\n{file_path}\n"
                    f"AR码ID: {aruco_id}\n"
                    f"包含区域数: {len(self.rel_positions)}"
                )
            except Exception as e:
                QMessageBox.critical(
                    self, 
                    "导出错误", 
                    f"保存文件失败:\n{str(e)}"
                )
        else:
            self.statusBar().showMessage("取消导出")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = PointCalculator()
    window.show()
    sys.exit(app.exec_())