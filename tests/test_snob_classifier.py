import unittest
import pandas as pd
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
import snob

class TestSNOBClassifier(unittest.TestCase):
    def setUp(self):
        # ensure we're accessing the right path for test data
        self.base_dir = os.path.join(os.path.dirname(__file__), "..")
        self.csv_path = os.path.join(self.base_dir, "examples", "5r8c.csv")
        self.model_path = os.path.join("/tmp", "5r8c_test.mod")
        
        self.train_data = pd.read_csv(self.csv_path)

    def test_fit_and_predict(self):
        sfc = snob.SNOBClassifier(
            name='5r8c_test',
            attrs={
                'v1': 'real',
                'v2': 'real',
                'v3': 'real',
                'v4': 'real',
                'v5': 'real',
            },
            cycles=50, steps=50, moves=4, seed=1234567
        )

        # Test fitting the model
        sfc.fit(self.train_data)

        # Test number of classes
        leaves = [c['id'] for c in sfc.get_classes() if c['type'] == 2]
        self.assertEqual(len(leaves), 8)
        
        # Test saving the model
        sfc.save_model(self.model_path)
        self.assertTrue(os.path.exists(self.model_path))

        # Test predicting
        pred = sfc.predict(self.train_data)
        self.assertIsNotNone(pred)
        self.assertEqual(len(pred), len(self.train_data))

        # Test getting classes
        leaves = [c['id'] for c in sfc.get_classes() if c['type'] == 2]
        self.assertEqual(len(leaves), 8)

    def tearDown(self):
        if os.path.exists(self.model_path):
            os.remove(self.model_path)

if __name__ == '__main__':
    unittest.main()
