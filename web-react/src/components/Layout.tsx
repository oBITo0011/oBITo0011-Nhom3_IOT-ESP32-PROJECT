import { Outlet, useNavigate, useLocation } from 'react-router-dom'
import { AppBar, Toolbar, Typography, Button, Box, Drawer, List, ListItem, ListItemButton, ListItemIcon, ListItemText, CssBaseline, IconButton, Tooltip } from '@mui/material'
import DashboardIcon from '@mui/icons-material/Dashboard'
import ListAltIcon from '@mui/icons-material/ListAlt'
import LogoutIcon from '@mui/icons-material/Logout'
import DarkModeOutlinedIcon from '@mui/icons-material/DarkModeOutlined'
import LightModeOutlinedIcon from '@mui/icons-material/LightModeOutlined'
import { useState } from 'react'
import { useAuth } from '../contexts/AuthContext'

const drawerWidth = 240

export default function Layout() {
  const { logout, role, token } = useAuth()
  const navigate = useNavigate()
  const location = useLocation()
  const [colorMode, setColorMode] = useState<'day' | 'night'>(() => localStorage.getItem('iot-color-mode') === 'night' ? 'night' : 'day')

  const toggleColorMode = () => setColorMode((current) => {
    const next = current === 'day' ? 'night' : 'day'
    localStorage.setItem('iot-color-mode', next)
    return next
  })

  if (!token) return null

  const menuItems = [
    { text: 'Dashboard', icon: <DashboardIcon />, path: '/' },
    { text: 'History Logs', icon: <ListAltIcon />, path: '/logs' },
  ]

  return (
    <Box sx={{ display: 'flex', minHeight: '100vh', bgcolor: colorMode === 'night' ? '#0E1C34' : '#F5F9FF' }}>
      <CssBaseline />
      <AppBar position="fixed" elevation={0} sx={{ zIndex: (theme) => theme.zIndex.drawer + 1, bgcolor: colorMode === 'night' ? '#10213D' : 'rgba(255,255,255,.9)', color: colorMode === 'night' ? '#EAF1FF' : '#172B4D', borderBottom: colorMode === 'night' ? '1px solid rgba(143,179,255,.16)' : '1px solid rgba(91,140,255,.12)', backdropFilter: 'blur(14px)' }}>
        <Toolbar>
          <Typography variant="h6" noWrap component="div" sx={{ flexGrow: 1, fontWeight: 800, fontSize: 17 }}>
            IoT Smart Environment Pro
          </Typography>
          <Typography variant="body1" sx={{ mr: 2 }}>
            Role: {role}
          </Typography>
          <Tooltip title={colorMode === 'day' ? 'Chế độ ban đêm' : 'Chế độ ban ngày'}>
            <IconButton onClick={toggleColorMode} color="inherit" sx={{ mr: 1 }}>
              {colorMode === 'day' ? <DarkModeOutlinedIcon /> : <LightModeOutlinedIcon />}
            </IconButton>
          </Tooltip>
          <Button color="inherit" onClick={logout} startIcon={<LogoutIcon />} sx={{ fontWeight: 700 }}>
            Logout
          </Button>
        </Toolbar>
      </AppBar>
      <Drawer
        variant="permanent"
        sx={{
          width: drawerWidth,
          flexShrink: 0,
          [`& .MuiDrawer-paper`]: { width: drawerWidth, boxSizing: 'border-box', bgcolor: colorMode === 'night' ? '#10213D' : 'rgba(255,255,255,.88)', color: colorMode === 'night' ? '#D9E7FF' : '#172B4D', borderRight: colorMode === 'night' ? '1px solid rgba(143,179,255,.16)' : '1px solid rgba(91,140,255,.12)' },
        }}
      >
        <Toolbar />
        <Box sx={{ overflow: 'auto' }}>
          <List>
            {menuItems.map((item) => (
              <ListItem key={item.text} disablePadding>
                <ListItemButton 
                  selected={location.pathname === item.path}
                  onClick={() => navigate(item.path)}
                >
                  <ListItemIcon sx={{ color: location.pathname === item.path ? '#5B8CFF' : 'inherit' }}>
                    {item.icon}
                  </ListItemIcon>
                  <ListItemText primary={item.text} />
                </ListItemButton>
              </ListItem>
            ))}
          </List>
        </Box>
      </Drawer>
      <Box component="main" sx={{ flexGrow: 1, p: { xs: 1, md: 2 }, bgcolor: colorMode === 'night' ? '#0E1C34' : '#F5F9FF', minHeight: '100vh', transition: 'background-color .25s ease' }}>
        <Toolbar />
        <Outlet context={{ colorMode }} />
      </Box>
    </Box>
  )
}
