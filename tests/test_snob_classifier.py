import unittest
import pandas as pd
import os
import sys
import concurrent.futures

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
import snob

def fit_model(name, train_data):
    sfc = snob.SNOBClassifier(
        name=name,
        attrs={
            'v1': 'real',
            'v2': 'real',
            'v3': 'real',
            'v4': 'real',
            'v5': 'real',
        },
        cycles=50, steps=50, moves=4, seed=1234567,
    )
    sfc.fit(train_data)
    leaves = [c['id'] for c in sfc.get_classes() if c['type'] == 2]
    return len(leaves)

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

        leaves = [c['id'] for c in sfc.get_classes() if c['type'] == 2]
        self.assertEqual(len(leaves), 8)

    def test_concurrent_fits(self):
        with concurrent.futures.ProcessPoolExecutor(max_workers=2) as executor:
            future1 = executor.submit(fit_model, 'model_1', self.train_data)
            future2 = executor.submit(fit_model, 'model_2', self.train_data)

            leaves_1 = future1.result()
            leaves_2 = future2.result()

        self.assertEqual(leaves_1, 8)
        self.assertEqual(leaves_2, 8)

    def tearDown(self):
        if os.path.exists(self.model_path):
            os.remove(self.model_path)

if __name__ == '__main__':
    unittest.main()
