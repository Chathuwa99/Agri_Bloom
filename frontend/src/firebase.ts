import { initializeApp } from 'firebase/app'
import { getAuth } from 'firebase/auth'
import { getDatabase } from 'firebase/database'

const firebaseConfig = {
  apiKey: 'AIzaSyDo20_bhC1QcwWB86yiZA2mjvo9uDw1aC8',
  authDomain: 'agri-bloom-59b2a.firebaseapp.com',
  databaseURL: 'https://agri-bloom-59b2a-default-rtdb.firebaseio.com',
  projectId: 'agri-bloom-59b2a',
  storageBucket: 'agri-bloom-59b2a.firebasestorage.app',
  messagingSenderId: '83171406247',
  appId: '1:83171406247:web:791bb0955b0e4c6326c682',
  measurementId: 'G-MRKJ80B9G4',
}

const app = initializeApp(firebaseConfig)

export const db = getDatabase(app)
export const auth = getAuth(app)
