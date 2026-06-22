import type { ComponentType } from "react";
import {
  Settings,
  Folder,
  TerminalSquare,
  Calculator as CalcIcon,
  FileText,
  Activity,
  CalendarDays,
  HardDrive,
  Package,
  Info,
  Image as ImageIcon,
  Clock,
} from "lucide-react";

import { SettingsApp } from "./apps/SettingsApp";
import { FilesApp } from "./apps/FilesApp";
import { TerminalApp } from "./apps/TerminalApp";
import { CalculatorApp } from "./apps/CalculatorApp";
import { TextEditorApp } from "./apps/TextEditorApp";
import { SystemMonitorApp } from "./apps/SystemMonitorApp";
import { CalendarApp } from "./apps/CalendarApp";
import { DisksApp } from "./apps/DisksApp";
import { SoftwareApp } from "./apps/SoftwareApp";
import { AboutApp } from "./apps/AboutApp";
import { ImageViewerApp } from "./apps/ImageViewerApp";
import { ClocksApp } from "./apps/ClocksApp";

export type AppId =
  | "settings"
  | "files"
  | "terminal"
  | "calculator"
  | "editor"
  | "monitor"
  | "calendar"
  | "disks"
  | "software"
  | "about"
  | "photos"
  | "clocks";

export interface AppMeta {
  id: AppId;
  name: string;
  icon: ComponentType<{ className?: string }>;
  /** tailwind gradient classes for the icon tile */
  tile: string;
  component: ComponentType;
  defaultSize?: { width: number; height: number };
  /** show in the dock by default */
  pinned?: boolean;
}

export const APP_REGISTRY: Record<AppId, AppMeta> = {
  files: {
    id: "files",
    name: "Files",
    icon: Folder,
    tile: "from-[oklch(0.6_0.12_75)] to-[oklch(0.68_0.13_55)]",
    component: FilesApp,
    defaultSize: { width: 800, height: 540 },
    pinned: true,
  },
  settings: {
    id: "settings",
    name: "Settings",
    icon: Settings,
    tile: "from-[oklch(0.55_0.06_240)] to-[oklch(0.45_0.05_250)]",
    component: SettingsApp,
    defaultSize: { width: 860, height: 580 },
    pinned: true,
  },
  terminal: {
    id: "terminal",
    name: "Terminal",
    icon: TerminalSquare,
    tile: "from-[oklch(0.3_0.02_152)] to-[oklch(0.22_0.02_152)]",
    component: TerminalApp,
    defaultSize: { width: 720, height: 460 },
    pinned: true,
  },
  editor: {
    id: "editor",
    name: "Text Editor",
    icon: FileText,
    tile: "from-[oklch(0.6_0.1_220)] to-[oklch(0.52_0.11_235)]",
    component: TextEditorApp,
    defaultSize: { width: 760, height: 520 },
    pinned: true,
  },
  monitor: {
    id: "monitor",
    name: "System Monitor",
    icon: Activity,
    tile: "from-[oklch(0.6_0.18_25)] to-[oklch(0.55_0.18_15)]",
    component: SystemMonitorApp,
    defaultSize: { width: 780, height: 540 },
    pinned: true,
  },
  calculator: {
    id: "calculator",
    name: "Calculator",
    icon: CalcIcon,
    tile: "from-[oklch(0.5_0.12_290)] to-[oklch(0.45_0.13_300)]",
    component: CalculatorApp,
    defaultSize: { width: 340, height: 520 },
    pinned: true,
  },
  calendar: {
    id: "calendar",
    name: "Calendar",
    icon: CalendarDays,
    tile: "from-[oklch(0.6_0.15_30)] to-[oklch(0.55_0.16_20)]",
    component: CalendarApp,
    defaultSize: { width: 820, height: 560 },
  },
  clocks: {
    id: "clocks",
    name: "Clocks",
    icon: Clock,
    tile: "from-[oklch(0.55_0.1_200)] to-[oklch(0.5_0.11_215)]",
    component: ClocksApp,
    defaultSize: { width: 640, height: 460 },
  },
  photos: {
    id: "photos",
    name: "Photos",
    icon: ImageIcon,
    tile: "from-[oklch(0.65_0.14_330)] to-[oklch(0.6_0.15_345)]",
    component: ImageViewerApp,
    defaultSize: { width: 820, height: 560 },
  },
  disks: {
    id: "disks",
    name: "Disks",
    icon: HardDrive,
    tile: "from-[oklch(0.5_0.04_240)] to-[oklch(0.42_0.04_250)]",
    component: DisksApp,
    defaultSize: { width: 760, height: 500 },
  },
  software: {
    id: "software",
    name: "Software",
    icon: Package,
    tile: "from-[oklch(0.58_0.13_152)] to-[oklch(0.5_0.14_158)]",
    component: SoftwareApp,
    defaultSize: { width: 840, height: 580 },
  },
  about: {
    id: "about",
    name: "About AOS",
    icon: Info,
    tile: "from-[oklch(0.55_0.13_152)] to-[oklch(0.65_0.12_75)]",
    component: AboutApp,
    defaultSize: { width: 560, height: 520 },
  },
};

export const ALL_APPS: AppMeta[] = Object.values(APP_REGISTRY);
export const PINNED_APPS: AppMeta[] = ALL_APPS.filter((a) => a.pinned);
